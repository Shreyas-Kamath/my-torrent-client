#include <print>
#include <atomic>
#include <chrono>
#include <iostream>

class Stats {
public:

    std::atomic<int> connected_peers{};
    std::atomic<int> completed_pieces{};
    std::atomic<int> total_pieces{};
    std::atomic<size_t> downloaded_bytes{};
    std::atomic<size_t> uploaded_bytes{};
    std::atomic<size_t> total_size{};
    std::atomic<double> progress{};

    void display() const {
            auto peers = connected_peers.load();
            auto comp_pieces = completed_pieces.load();
            auto tot_pieces = total_pieces.load();
            auto down = downloaded_bytes.load();
            auto up = uploaded_bytes.load();
            auto total = total_size.load();
            auto prog = progress.load();

            std::print("\rPeers: {}, Pieces: {}/{}, Downloaded: {} MB, Uploaded: {} MB, Total size: {} MB, Progress: {:.2f}%", 
            peers, comp_pieces, tot_pieces, down / (1024 * 1024), up / (1024 * 1024), total / (1024 * 1024), ((double)comp_pieces / (double)tot_pieces) * 100);
            std::flush(std::cout);
    }
};