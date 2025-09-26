#include <PieceManager.hpp>

PieceManager::~PieceManager() {
    stop_writer_ = true;
    write_cv_.notify_all();
    if (writer_thread_.joinable()) writer_thread_.join();
}

void PieceManager::load_resume_data() {
    std::ifstream in(save_file_name_, std::ios::binary | std::ios::in);

    int piece_index, piece_count{};
    while (in.read(reinterpret_cast<char*>(&piece_index), sizeof(int))) {
        std::cout << "Found piece " << piece_index << "\n";
        auto& curr = pieces_[piece_index];
        auto curr_length = piece_length_for_index(piece_index);

        curr.bytes_written = curr_length;
        curr.is_complete = true;
        ++piece_count;
    }
    if (piece_count == num_pieces_) {
        std::cout << "All pieces already downloaded.\nStill missing files? Delete " << save_file_name_ << " and try again.\n";
        exit(0);
    }
}

void PieceManager::save_resume_data(int piece_index) {
    std::ofstream out(save_file_name_, std::ios::binary | std::ios::out | std::ios::app);
    out.write(reinterpret_cast<const char*>(&piece_index), sizeof(int));
}

void PieceManager::add_block(int piece_index, int begin, const unsigned char* block, size_t size) {
        std::scoped_lock<std::mutex> lock(piece_mutex_);
        auto& piece = pieces_[piece_index];

        auto block_index = begin / 16384; // find out which block has arrived

        if (!piece.block_received[block_index]) {
            std::copy(block, block + size, piece.data.begin() + begin);
            piece.block_received[block_index] = true;
            piece.bytes_written += size;

            // size_t received_blocks = std::count(piece.block_received.begin(), piece.block_received.end(), true);
            // std::cout << "Piece " << piece_index 
            // << ": received block " << block_index
            // << " (" << received_blocks << "/" << piece.block_received.size() << " blocks)\n";

            if (std::all_of(piece.block_received.begin(), piece.block_received.end(), [](bool b) { return b; }) && !piece.is_complete) 
            {
                if (verify_hash(piece_index, piece.data)) {
                    piece.is_complete = true;
                    {
                        std::lock_guard<std::mutex> lock(write_mutex_);
                        std::cout << "Piece " << piece_index << " written\n";
                        completed_pieces_.push(piece_index);
                    }
                    write_cv_.notify_one();
                    save_resume_data(piece_index);
                } else {
                    std::cerr << "Hash mismatch for piece " << piece_index << " (discarding)\n";
                    piece = PieceBuffer{}; // reset
                }
            }
        }
    }

bool PieceManager::verify_hash(int index, const std::vector<unsigned char>& data) {
        unsigned char digest[SHA_DIGEST_LENGTH];
        SHA1(data.data(), data.size(), digest);

        return std::equal(std::begin(digest), std::end(digest), piece_hashes_[index].begin());
}

void PieceManager::write_piece(int piece_index, const std::vector<unsigned char>& data) {
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

std::optional<int> PieceManager::fetch_next_piece(const boost::dynamic_bitset<>& peer_bitfield) {
    std::scoped_lock<std::mutex> lock(piece_mutex_);

    for (int i = 0; i < pieces_.size(); ++i) {
        maybe_init(i);
        if (!pieces_[i].is_complete && peer_bitfield.test(i)) {
            // at least one unrequested block
            auto& blocks = pieces_[i].block_requested;
            if (std::any_of(blocks.begin(), blocks.end(), [](bool b){ return !b; })) return i;
        }
    }
    return std::nullopt;
}

std::optional<int> PieceManager::next_block_offset(int piece_index) {
    std::scoped_lock<std::mutex> lock(piece_mutex_);
    auto& piece = pieces_[piece_index];

    if (piece.is_complete) return std::nullopt;

    for (int i = 0; i < piece.block_received.size(); ++i) {
        if (!piece.block_received[i] && !piece.block_requested[i]) {
            mark_block_requested(piece_index, i * 16384);
            return i * 16384;
        }
    }
    return std::nullopt;
}

void PieceManager::mark_block_requested(int piece_index, int offset) {
    auto& piece = pieces_[piece_index];
    piece.block_requested[offset / 16384] = true;
}

void PieceManager::maybe_init(int piece_index) {
            // lazy init
        auto& piece = pieces_[piece_index];
        if (piece.data.empty()) {
            auto curr_length = piece_length_for_index(piece_index);
            piece.data.resize(curr_length);  // allocate full size buffer
            size_t num_blocks = (curr_length + 16383) / 16384;
            piece.block_received.resize(num_blocks, false);
            piece.block_requested.resize(num_blocks, false);
        }
}

bool PieceManager::is_complete(int piece_index) {
    std::scoped_lock<std::mutex> lock(piece_mutex_);
    return pieces_[piece_index].is_complete;
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
                pieces_[front].data.clear();
                pieces_[front].data.shrink_to_fit();
                pieces_[front].block_received.clear();
                pieces_[front].block_received.shrink_to_fit();
            }
            lock.lock();
        }
    }
}