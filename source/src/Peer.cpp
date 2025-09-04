#include <Peer.hpp>

std::vector<Peer> parse_compact_peers(const std::string& peers_blob) {
    std::vector<Peer> peers;
    size_t count = peers_blob.size() / 6;

    for (size_t i = 0; i < count; ++i) {
        const unsigned char* data = reinterpret_cast<const unsigned char*>(peers_blob.data() + i * 6);

        std::ostringstream ip;
        ip << (int)data[0] << "."
           << (int)data[1] << "."
           << (int)data[2] << "."
           << (int)data[3];

        uint16_t port = (data[4] << 8) | data[5];

        peers.emplace_back(ip.str(), port);
    }
    return peers;
}
