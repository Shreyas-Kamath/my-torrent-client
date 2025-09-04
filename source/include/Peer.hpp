#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <sstream>

class Peer {
public:
    Peer(const std::string& ip, uint16_t port) : ip_(std::move(ip)), port_(port) {}

    const std::string& ip() const { return ip_; }
    uint16_t port() const { return port_; }

private:
    std::string ip_;
    uint16_t port_;
};

std::vector<Peer> parse_compact_peers(const std::string& peers_blob);