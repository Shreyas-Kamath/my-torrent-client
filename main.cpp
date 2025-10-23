#include <TorrentClient.hpp>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <torrent-file>\n";
        return 1;
    }

    TorrentClient tc(argv[1]);

    tc.run();
}

// std::atomic<bool> stop_signal{ false };

// void signal_handler(int) {
//     stop_signal = true;
// }

// void start_acceptor(tcp::acceptor& acceptor, PieceManager& pm, std::vector<std::shared_ptr<PeerConnection>>& connections);

// int main(int argc, char* argv[]) {
//     if (argc != 2) {
//         std::cerr << "Usage: " << argv[0] << " <torrent-file>\n";
//         return 1;
//     }

//     auto in = read_from_file(argv[1]);
//     auto metadata = parse_torrent(in);

//     Stats stats;

//     PieceManager pm(metadata.total_size,
//                     metadata.piece_hashes.size(),
//                     metadata.piece_length,
//                     metadata.piece_hashes,
//                     metadata.name,
//                     stats);
//     pm.init_files(metadata.files);

//     boost::asio::io_context io;
//     tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 6881));

//     std::vector<std::shared_ptr<PeerConnection>> connections;

//     // --- Initialize trackers once ---
//     std::vector<std::shared_ptr<BaseTracker>> trackers;
//     for (const auto& tier : metadata.announce_list)
//         for (const auto& url : tier)
//             trackers.push_back(make_tracker(url));

//     std::unordered_set<Peer, PeerHash> peer_pool;

//     // --- Timers ---
//     auto announce_timer = std::make_shared<boost::asio::steady_timer>(io);
//     auto stats_timer = std::make_shared<boost::asio::steady_timer>(io, std::chrono::seconds(1));

//     // --- announce function ---
//     std::function<void()> announce_fn;
//     announce_fn = [&]() {
//         for (auto& tracker : trackers) {
//             try {
//                 auto response = tracker->announce(metadata.info_hash,
//                                                   "-CT0001-123456789012",
//                                                   stats.uploaded_bytes,
//                                                   stats.downloaded_bytes,
//                                                   stats.total_size);

//                 for (auto& peer : response.peers) {
//                     if (peer_pool.insert(peer).second) {
//                         auto conn = std::make_shared<PeerConnection>(
//                             io, peer, metadata.info_hash, "-CT0001-123456789012", pm
//                         );
//                         connections.push_back(conn);
//                         conn->start();
//                     }
//                 }

//             } catch (const std::exception& e) {
//                 std::cerr << "Tracker " << tracker->name() << " failed: " << e.what() << "\n";
//             }
//         }

//         // Schedule next announce
//         announce_timer->expires_after(std::chrono::seconds(180));
//         announce_timer->async_wait([&](const boost::system::error_code& ec) {
//             if (!ec) announce_fn();
//         });
//     };

//     // --- Stats timer ---
//     std::function<void()> stats_fn;
//     stats_fn = [&, stats_timer]() {
//         stats.display();
//         stats_timer->expires_after(std::chrono::seconds(1));
//         stats_timer->async_wait([&](const boost::system::error_code& ec) {
//             if (!ec) stats_fn();
//         });
//     };

//     // --- Signal handling ---
//     std::signal(SIGINT, [](int) { stop_signal = true; });

//     announce_fn();  // first announce
//     stats_fn();     // start stats display

//     // --- Event loop ---
//     while (!stop_signal) {
//         io.run_one();
//     }

//     std::cout << "\nShutting down...\n";
//     announce_timer->cancel();
//     stats_timer->cancel();
//     io.stop();

//     for (auto& conn : connections) conn->stop();
// }
