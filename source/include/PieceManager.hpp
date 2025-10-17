#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <openssl/sha.h>   // from OpenSSL
#include <iostream>
#include <iomanip>
#include <sstream>
#include <array>
#include <filesystem>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <optional>
#include <algorithm>
#include <ranges>

#include <TorrentFile.hpp>
#include <Stats.hpp>

#include <boost/dynamic_bitset.hpp>

class PeerConnection;

class PieceManager {
public:
    PieceManager(size_t total_size,
                 size_t num_pieces,
                 size_t piece_length,
                 const std::vector<std::array<unsigned char, 20>>& piece_hashes,
                 const std::string& torrent_name,
                 Stats& stats)
        : total_length_(total_size),
          num_pieces_(num_pieces),
          piece_length_(piece_length),
          piece_hashes_(std::move(piece_hashes)),
          stats_(stats)
    { 
        pieces_.resize(num_pieces);
        my_bitfield_.resize((num_pieces + 7) / 8, 0);

        stats_.total_pieces.store(num_pieces, std::memory_order_relaxed); 
        stats_.total_size.store(total_length_, std::memory_order_relaxed);

        std::cout << num_pieces << " pieces found.\n";
        save_file_name_ = torrent_name + ".fastresume";

        if (std::filesystem::exists(save_file_name_)) load_resume_data();
        else std::ofstream out(save_file_name_, std::ios::binary | std::ios::trunc);

        writer_thread_ = std::thread(&PieceManager::writer_thread_func, this);
        timeout_thread_ = std::thread(&PieceManager::timeout_thread_func, this);
    }
    
    ~PieceManager();

    void add_block(int piece_index, int begin, std::span<const unsigned char>);
    size_t piece_length_for_index(int piece_index) const;
    void init_files(const std::vector<TorrentFile>& files);

    std::optional<std::pair<int, int>> next_block_request(const boost::dynamic_bitset<>& peer_bitfield, std::chrono::steady_clock::time_point sent_time, std::weak_ptr<PeerConnection> peer);

    void maybe_init(int piece_index);
    bool is_complete(int piece_index);

    size_t num_pieces_;

    void add_to_peer_list(std::weak_ptr<PeerConnection> peer); // peer list
    std::vector<uint8_t> get_my_bitfield();
    
private:
    std::string save_file_name_;

    // read resume data if available
    std::mutex resume_file_mutex_;
    void load_resume_data();
    void save_resume_data(int piece_index);

    enum class BlockState { NotRequested, Requested, Received };

    struct InFlightBlock {
        std::chrono::steady_clock::time_point sent_time{};
        std::weak_ptr<PeerConnection> peer{};
    };

    struct PieceBuffer {
        std::vector<unsigned char> data;
        std::vector<BlockState> block_status;
        std::vector<InFlightBlock> in_flight_blocks;
        size_t bytes_written = 0;
        bool is_complete = false;
    };

    struct OutputFile {        // do we need this at all?
        std::string path;
        size_t start, length;
    };

    std::vector<OutputFile> files_;

    std::vector<PieceBuffer> pieces_;
    size_t piece_length_;
    size_t total_length_;
    
    const std::vector<std::array<unsigned char, 20>>& piece_hashes_;

    // Writer thread machinery

    std::queue<int> completed_pieces_;
    std::mutex write_mutex_;
    std::mutex piece_mutex_;
    std::condition_variable write_cv_;
    std::thread writer_thread_;
    std::atomic<bool> stop_writer_{ false };

    bool verify_hash(int index, const std::vector<unsigned char>& data);
    void write_piece(int index, const std::vector<unsigned char>& data);
    void writer_thread_func();

    // timeout machinery
    std::thread timeout_thread_;
    std::atomic<bool> stop_timeout_{ false };
    std::condition_variable timeout_cv_;

    void timeout_thread_func();

    // Stats counter

    Stats& stats_;

    // peer list
    std::mutex peer_list_mutex_;
    std::vector<std::weak_ptr<PeerConnection>> peer_connections;
    void notify_all_peers(int piece_index);

    // file i/o
    std::mutex file_io_mutex_;

    // my bitfield
    void update_my_bitfield(int piece_index);
    std::mutex my_bitfield_mutex_;
    std::vector<uint8_t> my_bitfield_;
};
