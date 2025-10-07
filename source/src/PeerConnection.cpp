#include <PeerConnection.hpp>
#include <iostream>

void PeerConnection::start() {
    auto self = shared_from_this();

    tcp::endpoint endpoint(boost::asio::ip::make_address(peer_.ip()), peer_.port());
    socket_.async_connect(endpoint,
        [self](boost::system::error_code ec) {
            if (ec) {
                // std::cerr << "Failed to connect to "
                //           << self->peer_.ip() << ":" << self->peer_.port()
                //           << " -> " << ec.message() << "\n";
            } else {
                // std::cout << "Connected to "
                //           << self->peer_.ip() << ":" << self->peer_.port() << "\n";
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

    // std::cout << "Handshake sent (" << bytes << " bytes)\n";

    auto self = shared_from_this();
    boost::asio::async_read(socket_,
        boost::asio::buffer(handshake_buf_),
        [self](boost::system::error_code ec, std::size_t) {
            if (ec) {
                std::cerr << "Handshake receive failed for: " << self->peer_.ip() << ":" << self->peer_.port() << " - " << ec.message() << "\n";
                return;
            }

            // Verify info_hash
            std::string peer_info_hash(self->handshake_buf_.data() + 28, 20);
            if (peer_info_hash != std::string(reinterpret_cast<const char*>(self->info_hash_.data()), 20)) {
                std::cerr << "Info hash mismatch\n";
                return;
            }

            // std::cout << "Handshake verified with " 
            //           << self->peer_.ip() << ":" << self->peer_.port() << "\n";

            // try reading response
            self->read_message_length();
        });
}

void PeerConnection::read_message_length() {
    auto self = shared_from_this();
    boost::asio::async_read(socket_,
        boost::asio::buffer(length_buf_),
        [self](boost::system::error_code ec, std::size_t) {
            if (ec) {
                if (ec == boost::asio::error::eof) {
                    std::cout << "Peer has nothing to send\n";
                    return;
                }
                std::cerr << "Failed to read length: " << ec.message() << "\n";
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
    boost::asio::async_read(socket_,
        boost::asio::buffer(msg_buf_),
        [self, length](boost::system::error_code ec, std::size_t) {
            if (ec) {
                std::cerr << "Failed to read body: " << ec.message() << "\n";
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
    std::vector<unsigned char> payload(msg_buf_.begin() + 1, msg_buf_.end());

    switch (id) {
        case 0: 
            // std::cout << "Peer choked us\n"; 
            am_choked_ = true;
            break;                         // peer has choked us

        case 1: 
            am_choked_ = false;
            // std::cout << "Peer unchoked us\n";
            if (am_interested_) maybe_request_next();
            break;                       // peer has unchoked us

        case 2: std::cout << "Peer is interested\n"; break;                     // peer is interested in our pieces
        case 3: std::cout << "Peer is not interested\n"; break;                 // peer is not interested in our pieces
        case 4: handle_have(payload); break;                                    // peer has a piece
        case 5: handle_bitfield(payload); break;                                // peer's bitfield
        case 6: std::cout << "Received request\n"; break;                       // received a request
        case 7: handle_piece(payload); break;                                   // received piece data
        case 8: std::cout << "Received cancel\n"; break;                        // received a cancel
        case 9: std::cout << "Received port\n"; break;                          // received a port

        default: std::cerr << "Unknown msg id " << (int)id << "\n"; break;
    }
}

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
                std::cerr << "Failed to send interested: " << ec.message() << "\n";
                return;
            }
            std::cout << "Sent interested to "
                      << self->peer_.ip() << ":" << self->peer_.port() << "\n";
        });
}

void PeerConnection::send_request(int piece_index, int begin, int length) {
    auto self = shared_from_this();
    // std::cout << "Requesting piece " << piece_index << '\n';

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
                std::cerr << "Failed to send request: " << ec.message() << "\n";
                return;
            }
            // std::cout << "Sent request -> piece " << piece_index
            //           << ", begin " << begin
            //           << ", length " << length << "\n";
        });
        ++in_flight_blocks_;   
}


void PeerConnection::handle_have(const std::vector<unsigned char>& payload) {
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

void PeerConnection::handle_bitfield(const std::vector<unsigned char>& payload) {
    set_bitfield(payload);

    if (!am_interested_ && peer_has_needed_piece()) {
        am_interested_ = true;
        send_interested();
    }

    maybe_request_next();
}

bool PeerConnection::peer_has_needed_piece() {
    for (size_t i = 0; i < peer_bitfield_.size(); ++i) {
        if (peer_bitfield_.test(i) && !piece_manager_.is_complete(i)) {
            return true; // This peer has at least one piece we need
        }
    }
    return false;
}

void PeerConnection::set_bitfield(const std::vector<unsigned char>& payload) {
    for (size_t i = 0; i < payload.size(); ++i) {
        for (int bit = 7; bit >= 0; --bit) {
            if ((payload[i] >> bit) & 1) {
                int piece_index = i * 8 + (7 - bit);
                peer_bitfield_.set(piece_index);
            }
        }
    }
}

void PeerConnection::handle_piece(const std::vector<unsigned char>& payload) {
    if (payload.size() < 8) {
        std::cerr << "Invalid piece payload\n";
        return;
    }

    int piece_index =
        (payload[0] << 24) | (payload[1] << 16) | (payload[2] << 8) | payload[3];
    int begin =
        (payload[4] << 24) | (payload[5] << 16) | (payload[6] << 8) | payload[7];

    // std::cout << "Received block: piece " << piece_index
    //           << " begin " << begin
    //           << " length " << block.size() << "\n";

    // try storing the block now
    --in_flight_blocks_;
    piece_manager_.add_block(piece_index, begin, payload.data() + 8, payload.size() - 8);
    maybe_request_next();
}

void PeerConnection::maybe_request_next() {
    while (!am_choked_ && in_flight_blocks_ < max_in_flight_blocks) {
        auto now = std::chrono::steady_clock::now();
        if (auto req = piece_manager_.next_block_request(peer_bitfield_, now, weak_from_this())) {
            ++in_flight_blocks_;
            const auto& [piece_index, offset] = req.value();
            send_request(
                piece_index,
                offset,
                std::min(16384, (int)piece_manager_.piece_length_for_index(piece_index) - offset)
            );
        } else break;
    }
}

void PeerConnection::decrement_inflight_blocks() {
    this->in_flight_blocks_ -= 1;
}