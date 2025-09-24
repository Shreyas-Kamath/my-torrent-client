#include <iostream>

#include <Utils.hpp>
#include <Bencode.hpp>
#include <TorrentFile.hpp>
#include <TrackerFactory.hpp>
#include <Peer.hpp>
#include <PeerConnection.hpp>

std::vector<Peer> get_peers_from_tier(const std::vector<std::string>& tier, const std::array<uint8_t, 20>& info_hash) {
    for (const auto& url : tier) {
        try {
            auto tracker = make_tracker(url);
            auto peers = tracker->announce(info_hash, "-CT0001-123456789012");
            if (!peers.empty()) {
                std::cout << "Tracker succeeded: " << tracker->protocol() << " " << url << "\n";
                std::cout << peers.size() << " peers found.\n";
                return peers;
            } else {
                std::cout << "No peers from tracker " << url << ", moving to next...\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "Tracker failed: " << url << " (" << e.what() << ")\n";
        }
    }
    return {}; // no peers from this tier
}

bool connect_to_peers(boost::asio::io_context& io,
                      const std::vector<Peer>& peers,
                      const std::array<uint8_t, 20>& info_hash,
                      PieceManager& pm,
                      std::vector<std::shared_ptr<PeerConnection>>& connections)
{
    std::atomic<bool> any_connected = false;

    for (const auto& peer : peers) {
        auto conn = std::make_shared<PeerConnection>(
            io, peer, info_hash, "-CT0001-123456789012", pm,
            [&any_connected]() { any_connected = true; }  // callback
        );
        connections.push_back(conn);
        conn->start();
    }

    io.run();
    io.restart();

    return any_connected.load();
}

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

    std::vector<std::shared_ptr<PeerConnection>> connections;

    boost::asio::io_context io;

    for (const auto& tier : metadata.announce_list) {
        auto peers = get_peers_from_tier(tier, metadata.info_hash);
        if (!peers.empty()) {
            bool connected = connect_to_peers(io, peers, metadata.info_hash, pm, connections);
            if (connected) break; // move to next tier only if no peer connected
        }
    }
}
