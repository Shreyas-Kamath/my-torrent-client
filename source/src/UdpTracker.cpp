#include <UdpTracker.hpp>

TrackerResponse UdpTracker::announce(const std::array<uint8_t, 20>& infoHash, const std::string& peerId, const std::atomic<size_t>& uploaded, const std::atomic<size_t>& downloaded, const std::atomic<size_t>& total) {
    uint32_t interval{};
    try {
        uint32_t event = 0;

        auto up = uploaded.load();
        auto down = downloaded.load();
        auto tot = total.load();

        if (down == 0) event = 2; // started
        else if (down >= tot) event = 1; // completed

        const std::string host = parsed.host;
        const std::string port = parsed.port.empty() ? "6969" : parsed.port; // fallback

        boost::asio::io_context io;
        udp::resolver resolver(io);
        auto endpoints = resolver.resolve(udp::v4(), host, port);
        udp::endpoint ep = *endpoints.begin();
        udp::socket socket(io);
        socket.open(udp::v4());

        const int max_retries = 3;
        const std::chrono::seconds timeout(5);
        std::vector<Peer> peers;

        for (int attempt = 0; attempt < max_retries; ++attempt) {
            uint32_t connect_tx = rand32();
            std::vector<unsigned char> creq(16);
            write_be64(creq, 0, 0x41727101980ULL); // protocol ID
            write_be32(creq, 8, 0);                 // action = connect
            write_be32(creq, 12, connect_tx);      // transaction ID

            auto connect_resp = do_exchange(io, socket, ep, creq, 16, timeout);
            if (connect_resp.size() < 16) continue;

            uint32_t resp_action = read_be32(connect_resp.data() + 0);
            uint32_t resp_tx = read_be32(connect_resp.data() + 4);
            if (resp_action != 0 || resp_tx != connect_tx) continue;

            uint64_t connection_id = read_be64(connect_resp.data() + 8);

            // ANNOUNCE REQUEST
            uint32_t announce_tx = rand32();
            std::vector<unsigned char> areq(98);
            write_be64(areq, 0, connection_id);
            write_be32(areq, 8, 1); // announce
            write_be32(areq, 12, announce_tx);
            std::copy(infoHash.begin(), infoHash.end(), areq.begin() + 16);
            std::copy(peerId.begin(), peerId.begin() + std::min<size_t>(20, peerId.size()), areq.begin() + 36);
            write_be64(areq, 56, down); // downloaded
            write_be64(areq, 64, tot - down); // left
            write_be64(areq, 72, up); // uploaded
            write_be32(areq, 80, event); // event
            write_be32(areq, 84, 0); // IP default
            write_be32(areq, 88, rand32()); // key
            write_be32(areq, 92, static_cast<uint32_t>(-1)); // num_want
            write_be16(areq, 96, std::stoi(port));

            auto announce_resp = do_exchange(io, socket, ep, areq, 20, timeout);
            if (announce_resp.size() < 20) continue;

            uint32_t a_action = read_be32(announce_resp.data() + 0);
            uint32_t a_tx = read_be32(announce_resp.data() + 4);
            if (a_action != 1 || a_tx != announce_tx) continue;

            interval = read_be32(announce_resp.data() + 8);

            std::vector<unsigned char> peer_blob(announce_resp.begin() + 20, announce_resp.end());
            size_t num_peers = peer_blob.size() / 6;

            for (size_t i = 0; i < num_peers; ++i) {
                const unsigned char* p = peer_blob.data() + i * 6;
                std::string ip = std::to_string(p[0]) + "." + std::to_string(p[1]) + "." +
                                 std::to_string(p[2]) + "." + std::to_string(p[3]);
                uint16_t port = (static_cast<uint16_t>(p[4]) << 8) | static_cast<uint16_t>(p[5]);
                peers.emplace_back(boost::asio::ip::make_address(ip), port);
            }

            if (!peers.empty()) break; // stop retries if we got peers
        }

        return { peers, interval };

    } catch (const std::exception& e) {
        std::cerr << "UdpTracker announce exception: " << e.what() << "\n";
    }

    return {};
}

    std::vector<unsigned char> UdpTracker::do_exchange(boost::asio::io_context& io,
                                                  udp::socket& socket,
                                                  const udp::endpoint& ep,
                                                  const std::vector<unsigned char>& out_buf,
                                                  size_t min_resp_len,
                                                  std::chrono::seconds timeout)
    {
        std::promise<std::vector<unsigned char>> prom;
        auto fut = prom.get_future();

        boost::asio::deadline_timer timer(io);
        std::vector<unsigned char> resp_buf(1500); //

        // send
        socket.async_send_to(boost::asio::buffer(out_buf), ep,
            [&](const boost::system::error_code& ec, std::size_t /*sent*/) {
                if (ec) {
                    prom.set_value({});
                    return;
                }

                // start timer
                timer.expires_from_now(boost::posix_time::seconds(timeout.count()));
                timer.async_wait([&](const boost::system::error_code& ec2) {
                    if (!ec2) {
                        socket.cancel();
                    }
                });

                // receive
                udp::endpoint sender;
                socket.async_receive_from(boost::asio::buffer(resp_buf), sender,
                    [&](const boost::system::error_code& ec3, std::size_t len) {
                        timer.cancel();
                        if (ec3) {
                            prom.set_value({});
                            return;
                        }
                        if (len < min_resp_len) {
                            prom.set_value({});
                            return;
                        }
                        resp_buf.resize(len);
                        prom.set_value(resp_buf);
                    });
            });

        // run until receive or timeout
        io.run();
        // after run, reset io_context so it can be used again by caller
        io.restart();

        return fut.get();
    }