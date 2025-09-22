#include <iostream>

#include <Utils.hpp>
#include <Bencode.hpp>
#include <TorrentFile.hpp>
#include <TrackerFactory.hpp>
#include <Peer.hpp>
#include <PeerConnection.hpp>

int main(int argc, char* argv[]) {
    // Read and parse torrent file
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <torrent-file>\n";
        return 1;
    }
    
    auto in = read_from_file(std::string(argv[1]));
    auto metadata = parse_torrent(in);

    PieceManager pm(metadata.total_size,
                    metadata.piece_hashes.size(),
                    metadata.piece_length,
                    metadata.piece_hashes,
                    metadata.name
                );

    // Initialize output files for multi-file torrent
    pm.init_files(metadata.files);

    std::vector<std::shared_ptr<PeerConnection>> connections;

    for (const auto& tier : metadata.announce_list) {
        bool success = false;
        for (const auto& url : tier) {
            try {
                auto tracker = make_tracker(url);
                auto response = tracker->announce(percent_encode(metadata.info_hash), "-CT0001-123456789012");

                if (!response.empty()) {
                    std::cout << "Tracker succeeded: " << tracker->protocol() << " " << url << "\n";
                        
                    BEncodeParser parser(response);
                    boost::asio::io_context io;
                    
                    auto peers = parse_compact_peers(parser.parse().as_dict().at("peers"));
                    std::cout << peers.size() << " peers.\n";
                    if (peers.empty()) {
                        std::cout << "No peers found\nmoving to next tier...\n";
                        break;
                    }

                    for (const auto& peer : peers) {
                        auto conn = std::make_shared<PeerConnection>(io, peer, metadata.info_hash, "-CT0001-123456789012", pm);
                        connections.push_back(conn); // keep alive
                        conn->start();
                    }

                    io.run();
                    success = true;
                    break; // move to next tier
                }
            } catch (const std::exception& e) {
                std::cerr << "Tracker failed: " << url << " (" << e.what() << ")\n";
            }
        }
        if (success) break;
    }
}
