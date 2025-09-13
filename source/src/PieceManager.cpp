#include <PieceManager.hpp>

void PieceManager::add_block(int piece_index, int begin, const std::vector<unsigned char>& block) {
        auto& piece = pieces_[piece_index];
        if (piece.data.empty())
            piece.data.resize(piece_length_);  // allocate full size buffer

        std::copy(block.begin(), block.end(), piece.data.begin() + begin);
        piece.bytes_written += block.size();

        if (piece.bytes_written >= piece_length_ && !piece.is_complete) {
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

        return std::equal(std::begin(digest), std::end(digest),
                          piece_hashes_[index].begin());
    }

void PieceManager::write_piece(int index, const std::vector<unsigned char>& data) {
        outfile_.seekp(index * piece_length_, std::ios::beg);
        outfile_.write(reinterpret_cast<const char*>(data.data()), data.size());
        outfile_.flush();
    }