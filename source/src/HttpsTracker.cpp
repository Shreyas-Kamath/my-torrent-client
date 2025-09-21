#include <HttpsTracker.hpp>
#include <iostream>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace ssl   = net::ssl;
using tcp       = net::ip::tcp;

std::string HttpsTracker::announce(const std::string& infoHash, const std::string& peerId) {
    try {
        // Parse host and target from trackerUrl
        ParsedUrl parsed = parse_url(trackerUrl);

        std::string target = parsed.target += "?info_hash=" + infoHash +
                  "&peer_id="   + peerId +
                  "&port=6881&uploaded=0&downloaded=0&left=0&compact=1";      

        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};

        tcp::resolver resolver(ioc);
        beast::ssl_stream<tcp::socket> stream(ioc, ctx);

        // Set SNI hostname (many trackers require this)
        if(!SSL_set_tlsext_host_name(stream.native_handle(), parsed.host.c_str())) {
            throw beast::system_error(
                beast::error_code(static_cast<int>(::ERR_get_error()),
                                  net::error::get_ssl_category()));
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

        std::string body = beast::buffers_to_string(res.body().data());
        std::cout << "Received HTTPS tracker response\n";

        beast::error_code ec;
        stream.shutdown(ec);
        if (ec == net::error::eof) {
            // ignore EOF from shutdown
            ec = {};
        }
        if (ec != net::ssl::error::stream_truncated)
            throw beast::system_error{ec};
        
        return body;

    } catch (std::exception const& e) {
        std::cerr << "HttpsTracker error: " << e.what() << std::endl;
    }
}
