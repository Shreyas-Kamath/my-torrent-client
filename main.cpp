#include <print>
#include <iostream>

#include <Utils.hpp>
#include <Bencode.hpp>
#include <TorrentFile.hpp>
#include <TrackerFactory.hpp>

int main() {
    std::print("Hello world from C++23");

    auto in = read_from_file("ubuntu-25.04-desktop-amd64.iso.torrent");

    auto metadata = parse_torrent(in);

    for (const auto& tier : metadata.announce_list) {
        bool success = false;
        for (const auto& url : tier) {
            try {
                auto tracker = make_tracker(url);
                auto peers = tracker->announce(percent_encode(metadata.info_hash), "-CT0001-123456789012");

                if (!peers.empty()) {
                    std::cout << "Tracker succeeded: " << tracker->protocol()
                          << " " << url << "\n";
                    for (const auto& p : peers) {
                        std::cout << "  Peer: " << p << "\n";
                    }
                    success = true;
                    break; // move to next tier
                }
            } catch (const std::exception& e) {
                std::cerr << "Tracker failed: " << url
                          << " (" << e.what() << ")\n";
            }
        }
        if (success) break;
    }
}