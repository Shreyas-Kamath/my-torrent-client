#include <iostream>

#include <Utils.hpp>
#include <Bencode.hpp>
#include <TorrentFile.hpp>
#include <TrackerFactory.hpp>
#include <Peer.hpp>
#include <PeerConnection.hpp>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <torrent-file>\n";
        return 1;
    }

    auto in = read_from_file(argv[1]);
    auto metadata = parse_torrent(in);

    PieceManager pm(metadata.total_size,
                    metadata.piece_hashes.size(),
                    metadata.piece_length,
                    metadata.piece_hashes,
                    metadata.name);
    pm.init_files(metadata.files);

    boost::asio::io_context io;
    std::vector<std::shared_ptr<PeerConnection>> connections;

    // Trackers for reannounce
    std::vector<std::string> tracker_urls;
    for (const auto& tier : metadata.announce_list)
        tracker_urls.insert(tracker_urls.end(), tier.begin(), tier.end());

    auto announce_timer = std::make_shared<boost::asio::steady_timer>(io);

    std::function<void()> announce_fn;
    announce_fn = [&]() {
        for (auto& url : tracker_urls) {

            try {
                auto tracker = make_tracker(url);
                auto response = tracker->announce(metadata.info_hash, "-CT0001-123456789012");
                std::cout << "Tracker " << url << " returned " << response.peers.size() << " peers\n";

                for (auto& peer : response.peers) {
                    auto conn = std::make_shared<PeerConnection>(
                        io, peer, metadata.info_hash, "-CT0001-123456789012", pm
                    );
                    connections.push_back(conn);
                    conn->start();
                }

            } catch (const std::exception& e) {
                std::cerr << "Tracker " << url << " failed: " << e.what() << "\n";
            }
        }

        // Schedule next announce (reannounce) after 30s
        announce_timer->expires_after(std::chrono::seconds(120));
        announce_timer->async_wait([&](const boost::system::error_code& ec) {
            if (!ec) announce_fn();
        });
    };

    announce_fn();  // start first announce
    io.run();
}
