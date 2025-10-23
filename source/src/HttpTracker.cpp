#include <HttpTracker.hpp>

TrackerResponse HttpTracker::announce(const std::array<uint8_t, 20>& infoHash, const std::string& peerId, const std::atomic<size_t>& uploaded, const std::atomic<size_t>& downloaded, const std::atomic<size_t>& total)
{
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

        auto parsed_resp = parser.parse().as_dict();
        auto peers = parse_compact_peers(parsed_resp.at("peers"));

        std::optional<uint32_t> interval;

        auto it = parsed_resp.find("interval");
        if (it != parsed_resp.end()) interval = (uint32_t)it->second.as_int();

        return { peers, interval };
    }
    catch (std::exception const& e) {
        std::cerr << "HttpTracker error: " << e.what() << std::endl;
    }
    return {};
}
