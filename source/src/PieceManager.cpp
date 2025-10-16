#include <PieceManager.hpp>
#include <PeerConnection.hpp>

PieceManager::~PieceManager() {
    stop_writer_ = true;
    write_cv_.notify_all();
    if (writer_thread_.joinable()) writer_thread_.join();

    stop_timeout_ = true;
    timeout_cv_.notify_all();
    if (timeout_thread_.joinable()) timeout_thread_.join();
}

void PieceManager::load_resume_data() {
    std::ifstream in(save_file_name_, std::ios::binary | std::ios::in);

    int piece_index{}, piece_count{};

    while (in.read(reinterpret_cast<char*>(&piece_index), sizeof(int))) {
        auto& curr = pieces_[piece_index];
        auto curr_length = piece_length_for_index(piece_index);

        curr.bytes_written = curr_length;
        curr.is_complete = true;
        ++piece_count;
    }
    
    std::cout << "Found " << piece_count << '/' << num_pieces_ << " pieces\n";
    stats_.completed_pieces.fetch_add(piece_count, std::memory_order_relaxed);
    if (piece_count == num_pieces_) {
        std::cout << "All pieces already downloaded.\nStill missing files? Delete " << save_file_name_ << " and try again.\n";
        exit(0);
    }
}

void PieceManager::save_resume_data(int piece_index) {
    std::scoped_lock<std::mutex> lock(resume_file_mutex_);
    std::ofstream out(save_file_name_, std::ios::binary | std::ios::out | std::ios::app);
    out.write(reinterpret_cast<const char*>(&piece_index), sizeof(int));
}

void PieceManager::add_block(int piece_index, int begin, const std::span<const unsigned char> block) {
    bool piece_completed = false;

    {
        std::scoped_lock<std::mutex> lock(piece_mutex_);
        auto& piece = pieces_[piece_index];

        if (piece.is_complete) return;

        auto block_index = begin / 16384;

        if (piece.block_status[block_index] != BlockState::Received) {
            std::copy(block.begin(), block.end(), piece.data.begin() + begin);
            piece.block_status[block_index] = BlockState::Received;
            piece.bytes_written += block.size();
            stats_.downloaded_bytes += block.size();
        }

        // Check if the piece is now complete
        if (std::ranges::all_of(piece.block_status, [](auto b) { return b == BlockState::Received; })) {
            if (verify_hash(piece_index, piece.data)) {
                piece.is_complete = true;
                piece_completed = true;
            } else {
                // Hash mismatch, reset piece
                piece.data.clear();
                piece.block_status.clear();
                piece.in_flight_blocks.clear();
                piece.bytes_written = 0;
                maybe_init(piece_index);
            }
        }
    }

    // Signal to all peers
    if (piece_completed) {
        notify_all_peers(piece_index);

        {
            std::scoped_lock<std::mutex> lock(write_mutex_);
            stats_.completed_pieces.fetch_add(1, std::memory_order_relaxed);
            completed_pieces_.push(piece_index);
        }
        write_cv_.notify_one();

        save_resume_data(piece_index);
    }
}

bool PieceManager::verify_hash(int index, const std::vector<unsigned char>& data) {
        unsigned char digest[SHA_DIGEST_LENGTH];
        SHA1(data.data(), data.size(), digest);

        return std::equal(std::begin(digest), std::end(digest), piece_hashes_[index].begin());
}

void PieceManager::write_piece(int piece_index, const std::vector<unsigned char>& data) {
    std::scoped_lock<std::mutex> file_lock(file_io_mutex_);

    size_t piece_offset = piece_index * piece_length_;
    size_t remaining = data.size();
    size_t data_offset = 0;

    for (const auto& f : files_) {
        if (piece_offset >= f.start + f.length) continue;
        if (piece_offset + remaining <= f.start) break;

        size_t file_offset = piece_offset > f.start ? piece_offset - f.start : 0;
        size_t write_size = std::min(remaining, f.length - file_offset);

        std::ofstream out(f.path, std::ios::binary | std::ios::in | std::ios::out);
        out.seekp(file_offset, std::ios::beg);
        out.write(reinterpret_cast<const char*>(data.data() + data_offset), write_size);

        remaining -= write_size;
        data_offset += write_size;
        piece_offset += write_size;

        if (remaining == 0) break;
    }
}

size_t PieceManager::piece_length_for_index(int piece_index) const {
    return piece_index < num_pieces_ - 1 ? piece_length_ : total_length_ - piece_length_ * (num_pieces_ - 1);
}

void PieceManager::init_files(const std::vector<TorrentFile>& files) {
    files_.clear();
    size_t offset = 0;

    for (const auto& f : files) {
        std::filesystem::path file_path(f.path);
        if (file_path.has_parent_path()) {
            std::filesystem::create_directories(file_path.parent_path());
        }
        
        OutputFile out_file;
        out_file.path = f.path;
        out_file.start = offset;
        out_file.length = f.length;

        files_.push_back(out_file);

        // Create the file to ensure it exists (for some reason ios::out | ios::in doesnt create a file on windows)
        if (!std::filesystem::exists(f.path)) {
            std::ofstream create(f.path, std::ios::binary | std::ios::trunc);
            if (!create) throw std::runtime_error("Failed to create file: " + f.path);
        }
        offset += f.length;
    }
    total_length_ = offset;
}

std::optional<std::pair<int, int>> PieceManager::next_block_request(const boost::dynamic_bitset<>& peer_bitfield, std::chrono::steady_clock::time_point sent_time, std::weak_ptr<PeerConnection> peer) {
    std::scoped_lock<std::mutex> lock(piece_mutex_);

    for (int i = 0; i < pieces_.size(); ++i) {
        if (!pieces_[i].is_complete && peer_bitfield.test(i)) {
            maybe_init(i);
            auto& piece = pieces_[i];
            for (int j = 0; j < piece.block_status.size(); ++j) {
                if (piece.block_status[j] == BlockState::NotRequested) {
                    piece.block_status[j] = BlockState::Requested;
                    piece.in_flight_blocks[j].peer = peer;              // not sure if this is best
                    piece.in_flight_blocks[j].sent_time = sent_time;    // maybe we should do it AFTER sending the request?
                    return std::make_pair(i, j * 16384);
                }
            }
        }
    }
    return std::nullopt;
}

void PieceManager::maybe_init(int piece_index) {
        // lazy init
        auto& piece = pieces_[piece_index];
        if (piece.data.empty()) {
            piece.is_complete = false;
            auto curr_length = piece_length_for_index(piece_index);
            piece.data.resize(curr_length);  // allocate full size buffer
            size_t num_blocks = (curr_length + 16383) / 16384;
            piece.block_status.resize(num_blocks, BlockState::NotRequested);
            piece.in_flight_blocks.resize(num_blocks);
            piece.bytes_written = 0;
        }
}

bool PieceManager::is_complete(int piece_index) {
    std::scoped_lock<std::mutex> lock(piece_mutex_);
    return pieces_[piece_index].is_complete;
}

void PieceManager::add_to_peer_list(std::weak_ptr<PeerConnection> peer) {
    std::scoped_lock<std::mutex> lock(peer_list_mutex_);
    peer_connections.push_back(peer);
}

void PieceManager::writer_thread_func() {
    while (!stop_writer_) {
        std::unique_lock<std::mutex> lock(write_mutex_);
        write_cv_.wait(lock, [&]{
            return stop_writer_ || !completed_pieces_.empty();
        });

        while (!completed_pieces_.empty()) {
            int front = completed_pieces_.front(); completed_pieces_.pop();
            lock.unlock();
            write_piece(front, pieces_[front].data);
            // std::cout << "Piece " << front << " verified & written.\n";
            // clear data
            {
                std::scoped_lock<std::mutex> lock(piece_mutex_);
                auto& piece = pieces_[front];
                piece.is_complete = true;
                piece.data.clear();
                piece.data.shrink_to_fit();
                piece.block_status.clear();
                piece.block_status.shrink_to_fit();
                piece.in_flight_blocks.clear();
                piece.in_flight_blocks.shrink_to_fit();
            }
            lock.lock();
        }
    }
}

void PieceManager::timeout_thread_func() {
    while (!stop_timeout_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        auto now = std::chrono::steady_clock::now();
        // std::cout << "Checking for timeouts\n";
        for (auto& piece : pieces_) {
            std::scoped_lock lock(piece_mutex_);
            if (piece.is_complete) continue;
            for (size_t i = 0; i < piece.block_status.size(); ++i) {
                if (piece.block_status[i] == BlockState::Requested) {
                    if (now - piece.in_flight_blocks[i].sent_time > std::chrono::seconds(3)) {
                        piece.block_status[i] = BlockState::NotRequested;
                        if (auto peer = piece.in_flight_blocks[i].peer.lock()) {
                            peer->decrement_inflight_blocks(); // safely reduce peer in-flight count
                        }

                        piece.in_flight_blocks[i] = {}; // reset
                    }
                }
            }
        }
    }
}

void PieceManager::notify_all_peers(int piece_index) {
    int peer_count{};

    {
        std::scoped_lock<std::mutex> lock(peer_list_mutex_);
        for (auto& peer: peer_connections) {
            if (auto p = peer.lock()) {
                ++peer_count;
                p->signal_have(piece_index);
            }
        }
    }
    stats_.connected_peers.store(peer_count);
}
