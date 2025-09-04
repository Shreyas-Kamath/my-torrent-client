#include <PeerConnection.hpp>
#include <iostream>

void PeerConnection::start() {
    auto self = shared_from_this();

    tcp::endpoint endpoint(boost::asio::ip::make_address(peer_.ip()), peer_.port());
    socket_.async_connect(endpoint,
        [self](boost::system::error_code ec) {
            if (ec) {
                std::cerr << "Failed to connect to "
                          << self->peer_.ip() << ":" << self->peer_.port()
                          << " -> " << ec.message() << "\n";
            } else {
                std::cout << "Connected to "
                          << self->peer_.ip() << ":" << self->peer_.port() << "\n";
                self->do_handshake();
            }
        });
}

void PeerConnection::do_handshake() {
    auto self = shared_from_this();
    
    // Construct handshake message
    handshake_buf_[0] = 19; // pstrlen
    std::memcpy(&handshake_buf_[1], "BitTorrent protocol", 19); // pstr
    std::memset(&handshake_buf_[20], 0, 8); // reserved
    std::memcpy(&handshake_buf_[28], info_hash_.data(), 20); // info_hash
    std::memcpy(&handshake_buf_[48], peer_id_.data(), 20); // peer_id

    boost::asio::async_write(socket_,
        boost::asio::buffer(handshake_buf_),
        [self](boost::system::error_code ec, std::size_t bytes) {
            self->on_handshake(ec, bytes);
        });
}

void PeerConnection::on_handshake(boost::system::error_code ec, std::size_t bytes) {
    if (ec) {
        std::cerr << "Handshake send failed: " << ec.message() << "\n";
        return;
    }

    std::cout << "Handshake sent (" << bytes << " bytes)\n";

    auto self = shared_from_this();
    boost::asio::async_read(socket_,
        boost::asio::buffer(handshake_buf_),
        [self](boost::system::error_code ec, std::size_t) {
            if (ec) {
                std::cerr << "Handshake receive failed: " << ec.message() << "\n";
                return;
            }

            // Verify info_hash
            std::string peer_info_hash(self->handshake_buf_.data() + 28, 20);
            if (peer_info_hash != std::string(reinterpret_cast<const char*>(self->info_hash_.data()), 20)) {
                std::cerr << "Info hash mismatch\n";
                return;
            }

            std::cout << "Handshake verified with " 
                      << self->peer_.ip() << ":" << self->peer_.port() << "\n";
        });
}
