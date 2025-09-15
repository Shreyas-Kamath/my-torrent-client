#include <Peer.hpp>

std::vector<Peer> parse_compact_peers(const BEncodeValue& peers_blob) {
    std::vector<Peer> peers;

    if (peers_blob.is_string()) {
        std::cout << "Peers are in binary form\n";
        auto peers_string = peers_blob.as_string();
        size_t count = peers_string.size() / 6;

        for (size_t i = 0; i < count; ++i) {
            const unsigned char* data = reinterpret_cast<const unsigned char*>(peers_string.data() + i * 6);

            std::ostringstream ip;
            ip << (int)data[0] << "."
            << (int)data[1] << "."
            << (int)data[2] << "."
            << (int)data[3];

            uint16_t port = (data[4] << 8) | data[5];

            peers.emplace_back(ip.str(), port);
        }
    }

    else if (peers_blob.is_list()) {
        std::cout << "Peers are in BEncoded form\n";
        for (const auto& entry: peers_blob.as_list()) {
            const auto& d = entry.as_dict();
            auto ip = d.at("ip").as_string();
            auto port = d.at("port").as_int();
            peers.emplace_back(ip, (uint16_t)port);
        }
    }
    return peers;
}
