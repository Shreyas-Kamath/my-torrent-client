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

#include <TorrentFile.hpp>

#include <boost/dynamic_bitset.hpp>

class PieceManager {
public:
    PieceManager(size_t total_size,
                 size_t num_pieces,
                 size_t piece_length,
                 const std::vector<std::array<unsigned char, 20>>& piece_hashes)
        : total_length_(total_size),
          num_pieces_(num_pieces),
          piece_length_(piece_length),
          piece_hashes_(piece_hashes)
    { 
        pieces_.resize(num_pieces); 
        std::cout << num_pieces << " pieces found.\n";
        writer_thread_ = std::thread(&PieceManager::writer_thread_func, this);
    }
    
    ~PieceManager();

    void add_block(int piece_index, int begin, const std::vector<unsigned char>& block);
    size_t piece_length_for_index(int piece_index) const;
    void init_files(const std::vector<TorrentFile>& files);

    std::optional<int> fetch_next_piece(const boost::dynamic_bitset<> peer_bitfield);
    std::optional<int> next_block_offset(int piece_index);
    void mark_block_requested(int piece_index, int offset);
    void maybe_init(int piece_index);
    bool is_complete(int piece_index);

    size_t num_pieces_;
    
private:
    struct Block {
        int piece_index{}, offset{}, length{};
    };

    struct PieceBuffer {
        std::vector<unsigned char> data;
        std::vector<bool> block_received;
        std::vector<bool> block_requested;
        std::size_t bytes_written = 0;
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
};
