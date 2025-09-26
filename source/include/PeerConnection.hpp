#pragma once

#include <boost/asio.hpp>
#include <boost/dynamic_bitset.hpp>

#include <memory>
#include <string>
#include <queue>

#include <Peer.hpp>
#include <PieceManager.hpp>

using boost::asio::ip::tcp;

class PeerConnection : public std::enable_shared_from_this<PeerConnection> {
public:
    PeerConnection(boost::asio::io_context& io,
                   Peer peer,
                   std::array<uint8_t, 20ULL> info_hash,
                   std::string peer_id,
                   PieceManager& pm,
                   std::function<void()> on_connected = nullptr)
        : socket_(io),
          peer_(std::move(peer)),
          info_hash_(std::move(info_hash)),
          peer_id_(std::move(peer_id)),
          on_connected_(on_connected),
          piece_manager_(pm) {
            peer_bitfield_.resize(pm.num_pieces_, false);
          }

    void start();

private:
    void do_handshake();                                                    //
    void on_handshake(boost::system::error_code ec, std::size_t bytes);     //

    // if connected callback
    std::function<void()> on_connected_;

    tcp::socket socket_;                                                    //
    Peer peer_;                                                     //      //
    std::array<uint8_t, 20> info_hash_;                                     //  --> Connect to peer
    std::string peer_id_;                                    //             //

    std::array<char, 68> handshake_buf_; // 68 byte handshake               //
    PieceManager& piece_manager_;


    // -- Download data --

    struct BlockRequest {
        int piece_index, begin, length;
    };

    const int max_in_flight_blocks{500};
    int in_flight_blocks_{};

    void read_message_length();
    void read_message_body(size_t length);
    void handle_message();
    void send_interested();
    void send_request(int piece_index, int offset, int length);
    void handle_have(const std::vector<unsigned char>& payload);

    void handle_bitfield(const std::vector<unsigned char>& payload);
    void set_bitfield(const std::vector<unsigned char>& payload);
    bool peer_has_needed_piece();
    void handle_piece(const std::vector<unsigned char>& payload);
    void maybe_request_next();

    // Buffers

    std::array<char, 4> length_buf_; // 4 byte length prefix
    std::vector<char> msg_buf_; // Variable length message body

    // Peer state
    bool am_choked_ = true;
    bool peer_choked_ = true;
    bool am_interested_ = false;

    std::queue<BlockRequest> block_queue_; // peer has these pieces

    boost::dynamic_bitset<> peer_bitfield_; // Bitfield of pieces the peer has
};
