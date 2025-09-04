#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include "Peer.hpp"

using boost::asio::ip::tcp;

class PeerConnection : public std::enable_shared_from_this<PeerConnection> {
public:
    PeerConnection(boost::asio::io_context& io,
                   Peer peer,
                   std::array<uint8_t, 20ULL> info_hash,
                   std::string peer_id)
        : socket_(io),
          peer_(std::move(peer)),
          info_hash_(std::move(info_hash)),
          peer_id_(std::move(peer_id)) {}

    void start();

private:
    void do_handshake();
    void on_handshake(boost::system::error_code ec, std::size_t bytes);

    tcp::socket socket_;
    Peer peer_;
    std::array<uint8_t, 20> info_hash_;
    std::string peer_id_;

    std::array<char, 68> handshake_buf_; // 49+19 typical BT handshake
};
