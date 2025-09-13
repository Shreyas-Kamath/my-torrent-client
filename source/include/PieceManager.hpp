#include <vector>
#include <string>
#include <fstream>
#include <openssl/sha.h>   // from OpenSSL
#include <iostream>
#include <iomanip>
#include <sstream>
#include <array>

class PieceManager {
public:
    PieceManager(std::size_t num_pieces,
                 std::size_t piece_length,
                 const std::vector<std::array<unsigned char, 20>>& piece_hashes,
                 std::ofstream& outfile)
        : num_pieces_(num_pieces),
          piece_length_(piece_length),
          piece_hashes_(piece_hashes),
          outfile_(outfile) 
    {
        pieces_.resize(num_pieces);
    }

    void add_block(int piece_index, int begin, const std::vector<unsigned char>& block);

private:
    struct PieceBuffer {
        std::vector<unsigned char> data;
        std::size_t bytes_written = 0;
        bool is_complete = false;
    };

    std::vector<PieceBuffer> pieces_;
    std::size_t num_pieces_;
    std::size_t piece_length_;
    const std::vector<std::array<unsigned char, 20>>& piece_hashes_;
    std::ofstream& outfile_;

    bool verify_hash(int index, const std::vector<unsigned char>& data);

    void write_piece(int index, const std::vector<unsigned char>& data);
};
