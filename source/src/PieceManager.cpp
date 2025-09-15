#include <PieceManager.hpp>

void PieceManager::add_block(int piece_index, int begin, const std::vector<unsigned char>& block) {
        auto& piece = pieces_[piece_index];
        if (piece.data.empty()) {
            auto curr_length = piece_length_for_index(piece_index);
            piece.data.resize(curr_length);  // allocate full size buffer
            size_t num_blocks = (curr_length + 16383) / 16384;
            piece.block_received.resize(num_blocks, false);
        }

        auto block_size = block.size();
        auto block_index = begin / 16384; // find out which block has arrived

        if (!piece.block_received[block_index]) {
            std::copy(block.begin(), block.end(), piece.data.begin() + begin);
            piece.block_received[block_index] = true;
            piece.bytes_written += block_size;

            size_t received_blocks = std::count(piece.block_received.begin(), piece.block_received.end(), true);
            std::cout << "Piece " << piece_index 
            << ": received block " << block_index
            << " (" << received_blocks << "/" << piece.block_received.size() << " blocks)\n";
        }

        if (std::all_of(piece.block_received.begin(), piece.block_received.end(), [](bool b) { return b; }) && !piece.is_complete) {
            if (verify_hash(piece_index, piece.data)) {
                write_piece(piece_index, piece.data);
                piece.is_complete = true;
                std::cout << "Piece " << piece_index << " verified and written ✅\n";
            } else {
                std::cerr << "Hash mismatch for piece " << piece_index << " ❌ (discarding)\n";
                piece = PieceBuffer{}; // reset buffer
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

        // Pre-create the file to ensure it exists
        std::ofstream touch(f.path, std::ios::binary | std::ios::trunc);
        if (!touch.is_open()) {
            throw std::runtime_error("Failed to create file: " + f.path);
        }

        offset += f.length;
    }
    total_length_ = offset;
}