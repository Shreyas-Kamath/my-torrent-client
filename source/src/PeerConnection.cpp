#include <PeerConnection.hpp>

void PeerConnection::start() {
    auto self = shared_from_this();

    tcp::endpoint endpoint(peer_.addr(), peer_.port());
    
    socket_.async_connect(endpoint,
        [self](boost::system::error_code ec) {
            if (ec) self->stop();
            else self->do_handshake();
        });
}

void PeerConnection::start_inbound() {
    auto self = shared_from_this();

    std::print("got an inbound connection\n");
    boost::asio::async_read(socket_, boost::asio::buffer(handshake_buf_),
        [self](boost::system::error_code ec, size_t bytes) {
            if (ec) {
                std::cout << ec.what() << '\n';
                self->stop();
                return;
            }

            std::string peer_info_hash(self->handshake_buf_.data() + 28, 20);
            if (peer_info_hash != std::string(reinterpret_cast<const char*>(self->info_hash_.data()), 20)) {
                self->stop();
                return;
            }

            std::print("handshake matches!\n");
            self->send_handshake();
        });
}

void PeerConnection::send_handshake() {
    auto self = shared_from_this();

    // Construct handshake message
    handshake_buf_[0] = 19;                                     // pstrlen
    std::memcpy(&handshake_buf_[1], "BitTorrent protocol", 19); // pstr
    std::memset(&handshake_buf_[20], 0, 8);                     // reserved
    std::memcpy(&handshake_buf_[28], info_hash_.data(), 20);    // info_hash
    std::memcpy(&handshake_buf_[48], peer_id_.data(), 20);      // peer_id

    boost::asio::async_write(socket_,
        boost::asio::buffer(handshake_buf_),
        [self](boost::system::error_code ec, std::size_t bytes) {
            if (ec) {
                self->stop();
                return;
            }
            else self->on_inbound_handshake_complete();
        });    
}

void PeerConnection::on_inbound_handshake_complete() {
    std::print("inbound peer registered\n");
    piece_manager_.add_to_peer_list(weak_from_this());
    signal_bitfield();
    read_message_length();
}

// close connection and stop wasting resources
void PeerConnection::stop() {
    boost::system::error_code ec;
    if (socket_.is_open()) socket_.close(ec);
}

void PeerConnection::do_handshake() {
    auto self = shared_from_this();
    
    // Construct handshake message
    handshake_buf_[0] = 19;                                     // pstrlen
    std::memcpy(&handshake_buf_[1], "BitTorrent protocol", 19); // pstr
    std::memset(&handshake_buf_[20], 0, 8);                     // reserved
    std::memcpy(&handshake_buf_[28], info_hash_.data(), 20);    // info_hash
    std::memcpy(&handshake_buf_[48], peer_id_.data(), 20);      // peer_id

    boost::asio::async_write(socket_,
        boost::asio::buffer(handshake_buf_),
        [self](boost::system::error_code ec, std::size_t bytes) {
            if (ec) {
                self->stop();
                return;
            }
            else self->on_handshake(ec, bytes);
        });
}

void PeerConnection::on_handshake(boost::system::error_code ec, std::size_t bytes) {
    if (ec) return;
    // std::cout << "Handshake sent (" << bytes << " bytes)\n";

    auto self = shared_from_this();
    boost::asio::async_read(socket_,
        boost::asio::buffer(handshake_buf_),
        [self](boost::system::error_code ec, std::size_t) {
            if (ec) {
                self->stop();
                return;
            }

            // Verify info_hash
            std::string peer_info_hash(self->handshake_buf_.data() + 28, 20);
            if (peer_info_hash != std::string(reinterpret_cast<const char*>(self->info_hash_.data()), 20)) {
                self->stop();
                return;
            }

            // std::cout << "Handshake verified with " 
            //           << self->peer_.ip() << ":" << self->peer_.port() << "\n";

            // try reading response
            self->piece_manager_.add_to_peer_list(self->weak_from_this());
            self->signal_bitfield(); // send my bitfield
            self->read_message_length();
        });
}

void PeerConnection::read_message_length() {
    auto self = shared_from_this();

    boost::asio::async_read(socket_, boost::asio::buffer(length_buf_),
        [self](boost::system::error_code ec, std::size_t) {
            if (ec && ec != boost::asio::error::eof) {
                self->stop();
                return;
            }

            uint32_t len = (static_cast<uint8_t>(self->length_buf_[0]) << 24) |
                           (static_cast<uint8_t>(self->length_buf_[1]) << 16) |
                           (static_cast<uint8_t>(self->length_buf_[2]) << 8)  |
                           (static_cast<uint8_t>(self->length_buf_[3]));

            if (len == 0) {
                // Keep-alive (peer might be busy or slow connection)
                self->read_message_length();
                return;
            }

            self->msg_buf_.resize(len);
            self->read_message_body(len);
        });
}

void PeerConnection::read_message_body(std::size_t length) {
    auto self = shared_from_this();
    boost::asio::async_read(socket_, boost::asio::buffer(msg_buf_),
        [self, length](boost::system::error_code ec, std::size_t) {
            if (ec && ec != boost::asio::error::eof) {
                self->stop();
                return;
            }

            self->handle_message();

            // Loop again
            self->read_message_length();
        });
}

void PeerConnection::handle_message() {
    
    if (msg_buf_.empty()) return;

    uint8_t id = static_cast<uint8_t>(msg_buf_[0]);
    std::span<const unsigned char> payload(msg_buf_.data() + 1, msg_buf_.size() - 1);

    switch (id) {
        case 0: 
            am_choked_ = true;
            break;                                                              // peer has choked us

        case 1: 
            am_choked_ = false;
            if (am_interested_) maybe_request_next();
            break;                                                              // peer has unchoked us

        case 2: 
            peer_interested = true; 
            std::print("Peer is interested\n");
            signal_unchoke();
            break;                                                              // peer is interested in our pieces
        case 3:
            peer_interested = false;
            break;                                                              // peer is not interested in our pieces
        case 4: handle_have(payload); break;                                    // peer has a piece
        case 5: handle_bitfield(payload); break;                                // peer's bitfield
        case 6: std::print("Received a request\n"); handle_request(payload); break;                                 // received a request
        case 7: handle_piece(payload); break;                                   // received piece data
        case 8: std::cout << "Received cancel\n"; break;                        // received a cancel
        case 9: std::cout << "Received port\n"; break;                          // received a port

        default: std::print("Unknown message id: {}\n", id); break;
    }
}

// indicate interest to a peer
void PeerConnection::send_interested() {
    auto self = shared_from_this();

    std::array<char, 5> msg{};
    // length prefix = 1 (message ID only)
    msg[0] = 0;
    msg[1] = 0;
    msg[2] = 0;
    msg[3] = 1;
    msg[4] = 2; // message ID = interested

    boost::asio::async_write(socket_, boost::asio::buffer(msg),
        [self](boost::system::error_code ec, std::size_t /*bytes*/) {
            if (ec) {
                self->stop();
                return;
            }
        });
}

// ask for a block
void PeerConnection::send_request(int piece_index, int begin, int length) {
    auto self = shared_from_this();

    std::array<char, 17> msg{};

    // length prefix = 13
    msg[0] = 0;
    msg[1] = 0;
    msg[2] = 0;
    msg[3] = 13;

    msg[4] = 6; // message ID = request

    // piece index
    msg[5] = (piece_index >> 24) & 0xFF;
    msg[6] = (piece_index >> 16) & 0xFF;
    msg[7] = (piece_index >> 8) & 0xFF;
    msg[8] = piece_index & 0xFF;

    // begin
    msg[9]  = (begin >> 24) & 0xFF;
    msg[10] = (begin >> 16) & 0xFF;
    msg[11] = (begin >> 8) & 0xFF;
    msg[12] = begin & 0xFF;

    // length
    msg[13] = (length >> 24) & 0xFF;
    msg[14] = (length >> 16) & 0xFF;
    msg[15] = (length >> 8) & 0xFF;
    msg[16] = length & 0xFF;

    boost::asio::async_write(socket_, boost::asio::buffer(msg),
        [self, piece_index, begin, length](boost::system::error_code ec, std::size_t /*bytes*/) {
            if (ec) {
                self->stop();
                return;
            }
        });
        in_flight_blocks_.fetch_add(1, std::memory_order_relaxed);  
}

// peer informs that they have a piece
void PeerConnection::handle_have(const std::span<const unsigned char> payload) {
    if (payload.size() < 4) return;

    int piece_index =
        (static_cast<unsigned char>(payload[0]) << 24) |
        (static_cast<unsigned char>(payload[1]) << 16) |
        (static_cast<unsigned char>(payload[2]) << 8)  |
        (static_cast<unsigned char>(payload[3]));

    // std::cout << "Peer " << peer_.ip() << ":" << peer_.port()
    //           << " has piece " << piece_index << "\n";
    
    peer_bitfield_.set(piece_index);

    // Send interested if not already
    if (!am_interested_) {
        am_interested_ = true;
        send_interested();
    }

    // Try requesting immediately if unchoked
    maybe_request_next();
}

// process bitfield and decide interest
void PeerConnection::handle_bitfield(const std::span<const unsigned char> payload) {
    set_bitfield(payload);

    if (!am_interested_ && peer_has_needed_piece()) {
        am_interested_ = true;
        send_interested();
    }

    maybe_request_next();
}

// condition to check whether sending requests to this peer is redundant
bool PeerConnection::peer_has_needed_piece() {
    for (size_t i = 0; i < peer_bitfield_.size(); ++i) {
        if (peer_bitfield_.test(i) && !piece_manager_.is_complete(i)) {
            return true; // This peer has at least one piece we need
        }
    }
    return false;
}

// set a bitfield for my reference, 
void PeerConnection::set_bitfield(const std::span<const unsigned char> payload) {
    for (size_t i = 0; i < payload.size(); ++i) {
        for (int bit = 7; bit >= 0; --bit) {
            if ((payload[i] >> bit) & 1) {
                int piece_index = i * 8 + (7 - bit);
                peer_bitfield_.set(piece_index);
            }
        }
    }
}

// incoming piece
void PeerConnection::handle_piece(const std::span<const unsigned char> payload) {
    if (payload.size() < 8) {
        std::cerr << "Invalid piece payload\n";
        return;
    }

    int piece_index = (payload[0] << 24) | (payload[1] << 16) | (payload[2] << 8) | payload[3];
    int begin = (payload[4] << 24) | (payload[5] << 16) | (payload[6] << 8) | payload[7];

    // try storing the block now
    in_flight_blocks_.fetch_sub(1, std::memory_order_relaxed);
    piece_manager_.add_block(piece_index, begin, payload.subspan(8));

    maybe_request_next();
}

// try request
void PeerConnection::maybe_request_next() {
    while (!am_choked_ && in_flight_blocks_ < max_in_flight_blocks) {
        auto now = std::chrono::steady_clock::now();
        if (auto req = piece_manager_.next_block_request(peer_bitfield_, now, weak_from_this())) {
            const auto& [piece_index, offset] = req.value();
            send_request(
                piece_index,
                offset,
                std::min(16384, (int)piece_manager_.piece_length_for_index(piece_index) - offset)
            );
        } else break;
    }
}

// confirmation from the piece manager that a block is missed
void PeerConnection::decrement_inflight_blocks() {
    in_flight_blocks_.fetch_sub(1, std::memory_order_relaxed);
}

// signal to the peer that I have this piece
void PeerConnection::signal_have(int piece_index) {
    auto self = shared_from_this();

    std::array<unsigned char, 9> msg{};

    uint32_t len = boost::endian::native_to_big(5);
    uint32_t index = boost::endian::native_to_big(piece_index);

    std::memcpy(msg.data(), &len, 4);       // length prefix
    msg[4] = 4;                             // message ID -- HAVE
    std::memcpy(msg.data() + 5, &index, 4); // piece index in big endian

    boost::asio::async_write(socket_, boost::asio::buffer(msg),
        [self, piece_index](boost::system::error_code ec, size_t bytes) {}
    );
} 

void PeerConnection::signal_bitfield() {
    auto self = shared_from_this();

    auto my_bitfield = piece_manager_.get_my_bitfield();
    
    uint32_t msg_len = boost::endian::native_to_big(1 + my_bitfield.size());

    std::vector<uint8_t> buffer;
    buffer.reserve(4 + msg_len);

    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&msg_len), reinterpret_cast<uint8_t*>(&msg_len) + 4);
    buffer.push_back(5); // id = bitfield
    buffer.insert(buffer.end(), my_bitfield.begin(), my_bitfield.end());

    // move the buffer into the lambda to prevent lifetime issues
    // and mark the lambda mutable to make the compiler happy

    boost::asio::async_write(socket_, boost::asio::buffer(buffer),
        [self, buf = std::move(buffer)](boost::system::error_code ec, size_t bytes) mutable {});

    // our job is done, we don't care whether the bitfield reaches the peer or not
}   

void PeerConnection::signal_unchoke() {
    auto self = shared_from_this();
    peer_choked = false;
    
    std::array<uint8_t, 5> msg{};

    uint32_t len = boost::endian::native_to_big(1);
    std::memcpy(msg.data(), &len, 4);
    msg[4] = 1; // id = unchoke

    boost::asio::async_write(socket_, boost::asio::buffer(msg),
        [self](boost::system::error_code ec, size_t bytes) {
            if (!ec) std::print("Unchoked a peer\n");
        });

    // again, we don't care if the message is received by the peer or not
}

// peer is requesting a piece (PIECE_INDEX, OFFSET, LENGTH)
void PeerConnection::handle_request(const std::span<const unsigned char> payload) {
    if (payload.size() < 12) return;

    uint32_t piece_index = boost::endian::big_to_native(*reinterpret_cast<const uint32_t*>(payload.data()));
    uint32_t begin = boost::endian::big_to_native(*reinterpret_cast<const uint32_t*>(payload.data() + 4));
    uint32_t length = boost::endian::big_to_native(*reinterpret_cast<const uint32_t*>(payload.data() + 8));

    auto block = piece_manager_.fetch_block(piece_index, begin, length);
    if (block.empty()) return;

    auto self = shared_from_this();

    uint32_t msg_len = boost::endian::native_to_big(9 + block.size());

    std::vector<uint8_t> buffer;
    buffer.reserve(4 + msg_len);

    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&msg_len), reinterpret_cast<uint8_t*>(&msg_len) + 4);
    buffer.push_back(7); // id - piece

    uint32_t be_index = boost::endian::native_to_big(piece_index);
    uint32_t be_begin = boost::endian::native_to_big(begin);

    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&be_index), reinterpret_cast<uint8_t*>(&be_index) + 4);
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&be_begin), reinterpret_cast<uint8_t*>(&be_begin) + 4);
    buffer.insert(buffer.end(), block.begin(), block.end());

    boost::asio::async_write(socket_, boost::asio::buffer(buffer),
        [self, b = std::move(buffer)](boost::system::error_code ec, size_t bytes) mutable {
            if (!ec) std::print("Block uploaded\n");
        });
}