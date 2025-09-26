#include <HttpTracker.hpp>

std::vector<Peer> HttpTracker::announce(const std::array<uint8_t, 20>& infoHash, const std::string& peerId)
{
    try {
        ParsedUrl parsed = parse_url(trackerUrl);

        std::string target = parsed.target + "?info_hash=" + percent_encode(infoHash) +
                             "&peer_id="   + peerId +
                             "&port=6881&uploaded=0&downloaded=0&left=0&compact=1";

        net::io_context ioc;
        tcp::resolver resolver(ioc);
        tcp::socket socket(ioc);

        auto const results = resolver.resolve(parsed.host, parsed.port);
        net::connect(socket, results.begin(), results.end());

        http::request<http::string_body> req{http::verb::get, target, 11};
        req.set(http::field::host, parsed.host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        http::write(socket, req);

        beast::flat_buffer buffer;
        http::response<http::dynamic_body> res;
        http::read(socket, buffer, res);

        if (res.result() != http::status::ok) {
            std::cerr << "Tracker returned HTTP error: " 
                    << res.result_int() << " " 
                    << res.reason() << "\n";
            return {};
        }
        
        std::string body = beast::buffers_to_string(res.body().data());
        // std::cout << "Tracker response: " << body << "\n";

        beast::error_code ec;
        socket.shutdown(tcp::socket::shutdown_both, ec);

        BEncodeParser parser(body);

        return parse_compact_peers(parser.parse().as_dict().at("peers"));
    }
    catch (std::exception const& e) {
        std::cerr << "HttpTracker error: " << e.what() << std::endl;
    }
    return {};
}
