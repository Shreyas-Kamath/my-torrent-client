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
                 const std::vector<std::array<unsigned char, 20>>& piece_hashes,
                 const std::string& torrent_name)
        : total_length_(total_size),
          num_pieces_(num_pieces),
          piece_length_(piece_length),
          piece_hashes_(std::move(piece_hashes))
    { 
        pieces_.resize(num_pieces); 
        std::cout << num_pieces << " pieces found.\n";
        writer_thread_ = std::thread(&PieceManager::writer_thread_func, this);
        save_file_name_ = torrent_name + ".fastresume";

        if (std::filesystem::exists(save_file_name_)) load_resume_data();
        else std::ofstream out(save_file_name_, std::ios::binary | std::ios::trunc);
    }
    
    ~PieceManager();

    void add_block(int piece_index, int begin, const unsigned char* block, size_t size);
    size_t piece_length_for_index(int piece_index) const;
    void init_files(const std::vector<TorrentFile>& files);

    std::optional<std::pair<int, int>> next_block_request(const boost::dynamic_bitset<>& peer_bitfield);

    void mark_block_requested(int piece_index, int offset);
    void maybe_init(int piece_index);
    bool is_complete(int piece_index);

    size_t num_pieces_;
    
private:
    std::string save_file_name_;

    // read resume data if available
    void load_resume_data();
    void save_resume_data(int piece_index);

    struct PieceBuffer {
        std::vector<unsigned char> data;
        boost::dynamic_bitset<> block_received;
        boost::dynamic_bitset<> block_requested;
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
