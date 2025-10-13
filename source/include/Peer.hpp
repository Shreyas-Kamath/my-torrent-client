#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#include <Bencode.hpp>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>

class Peer {
public:
    Peer(const boost::asio::ip::address& address, uint16_t port) : endpoint_(address, port) {}

    const boost::asio::ip::tcp::endpoint& endpoint() const { return endpoint_; }

    std::string ip() const { return endpoint_.address().to_string(); }
    uint16_t port() const { return endpoint_.port(); }

    bool operator==(const Peer& other) const {
        return this->ip() == other.ip() && this->port() == other.port();
    }

private:
    boost::asio::ip::tcp::endpoint endpoint_;
};

struct PeerHash {
    size_t operator()(const Peer& p) const noexcept {
        return std::hash<std::string>()(p.ip()) ^ (std::hash<uint16_t>()(p.port()) << 1);
    }
};

std::vector<Peer> parse_compact_peers(const BEncodeValue& peers_blob);