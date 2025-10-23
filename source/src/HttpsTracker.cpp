#include <HttpsTracker.hpp>
#include <iostream>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace ssl   = net::ssl;
using tcp       = net::ip::tcp;

TrackerResponse HttpsTracker::announce(const std::array<uint8_t, 20>& infoHash, const std::string& peerId, const std::atomic<size_t>& uploaded, const std::atomic<size_t>& downloaded, const std::atomic<size_t>& total) {
    try {
        std::string event;

        auto up = uploaded.load();
        auto down = downloaded.load();
        auto tot = total.load();

        if (down == 0) event = "started";
        else if (down >= tot) event = "completed";

        std::string target = parsed.target +
            "?info_hash=" + percent_encode(infoHash) +
            "&peer_id="   + peerId +
            "&port=6881&uploaded=" + std::to_string(up) + 
            "&downloaded=" + std::to_string(down) + 
            "&left=" + std::to_string(tot - down) +
            "&compact=1";
            
        if (!event.empty()) target += "&event=" + event;

        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};

        tcp::resolver resolver(ioc);
        beast::ssl_stream<tcp::socket> stream(ioc, ctx);

        // Set SNI hostname (many trackers require this)
        if(!SSL_set_tlsext_host_name(stream.native_handle(), parsed.host.c_str())) {
            throw beast::system_error(beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()));
        }

        auto const results = resolver.resolve(parsed.host, "443");
        net::connect(stream.next_layer(), results.begin(), results.end());

        stream.handshake(ssl::stream_base::client);

        // Build HTTP GET request
        http::request<http::string_body> req{http::verb::get, target, 11};
        req.set(http::field::host, parsed.host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        http::write(stream, req);

        beast::flat_buffer buffer;
        http::response<http::dynamic_body> res;
        http::read(stream, buffer, res);

        // std::cout << "Received HTTPS tracker response\n";
        std::string body = beast::buffers_to_string(res.body().data());

        beast::error_code ec;
        stream.shutdown(ec);
        if (ec == net::error::eof) {
            // ignore EOF from shutdown
            ec = {};
        }
        if (ec != net::ssl::error::stream_truncated) throw beast::system_error{ec};

        BEncodeParser parser(body);

        auto parsed_resp = parser.parse().as_dict();
        auto peers = parse_compact_peers(parsed_resp.at("peers"));

        std::optional<uint32_t> interval;

        auto it = parsed_resp.find("interval");
        if (it != parsed_resp.end()) interval = (uint32_t)it->second.as_int();

        return { peers, interval };

    } catch (std::exception const& e) {
        std::cerr << "HttpsTracker error: " << e.what() << std::endl;
    }
    return {};
}
