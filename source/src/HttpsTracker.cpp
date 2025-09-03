#include <HttpsTracker.hpp>
#include <iostream>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace ssl   = net::ssl;
using tcp       = net::ip::tcp;

std::vector<std::string> HttpsTracker::announce(const std::string& infoHash,
                                                const std::string& peerId) {
    std::vector<std::string> peers;

    try {
        // Parse host and target from trackerUrl
        auto pos = trackerUrl.find("://");
        std::string host = (pos != std::string::npos) ? trackerUrl.substr(pos + 3) : trackerUrl;
        auto slash = host.find('/');
        std::string target = (slash != std::string::npos) ? host.substr(slash) : "/";
        host = (slash != std::string::npos) ? host.substr(0, slash) : host;

        // Build query
        target += "?info_hash=" + infoHash +
                  "&peer_id="   + peerId +
                  "&port=6881&uploaded=0&downloaded=0&left=0&compact=1";

        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};

        tcp::resolver resolver(ioc);
        beast::ssl_stream<tcp::socket> stream(ioc, ctx);

        // Set SNI hostname (many trackers require this)
        if(!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            throw beast::system_error(
                beast::error_code(static_cast<int>(::ERR_get_error()),
                                  net::error::get_ssl_category()));
        }

        auto const results = resolver.resolve(host, "443");
        net::connect(stream.next_layer(), results.begin(), results.end());

        stream.handshake(ssl::stream_base::client);

        // Build HTTP GET request
        http::request<http::string_body> req{http::verb::get, target, 11};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        http::write(stream, req);

        beast::flat_buffer buffer;
        http::response<http::dynamic_body> res;
        http::read(stream, buffer, res);

        std::string body = beast::buffers_to_string(res.body().data());
        std::cout << "HTTPS Tracker response: " << body << "\n";

        peers.push_back(body);

        beast::error_code ec;
        stream.shutdown(ec);
        if (ec == net::error::eof) {
            // ignore EOF from shutdown
            ec = {};
        }
        if (ec)
            throw beast::system_error{ec};

    } catch (std::exception const& e) {
        std::cerr << "HttpsTracker error: " << e.what() << std::endl;
    }

    return peers;
}
