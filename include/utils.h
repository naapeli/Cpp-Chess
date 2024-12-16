typedef unsigned long long U64;

#ifndef utils
#define utils

#include <unordered_map>
#include <string>

using std::string;
using std::unordered_map;

namespace bitboard_utils
{
    void set_bit(U64 &bitboard, int square);
    int get_bit(U64 bitboard, int square);
    int pop_bit(U64 &bitboard, int square);

    int count_bits(U64 bitboard);
    int least_significant_bit_index(U64 bitboard);

    void print_bitboard(U64 bitboard);
}

namespace constants
{
    enum {
        a8, b8, c8, d8, e8, f8, g8, h8,
        a7, b7, c7, d7, e7, f7, g7, h7,
        a6, b6, c6, d6, e6, f6, g6, h6,
        a5, b5, c5, d5, e5, f5, g5, h5,
        a4, b4, c4, d4, e4, f4, g4, h4,
        a3, b3, c3, d3, e3, f3, g3, h3,
        a2, b2, c2, d2, e2, f2, g2, h2,
        a1, b1, c1, d1, e1, f1, g1, h1, no_square
    };
    extern const string square_to_coordinates[64];
    extern const string promotion_to_string;
    enum {white, black, both};  // side
    enum {P, N, B, R, Q, K, p, n, b, r, q, k};  // pieces
    enum {wk = 0b1000, wq = 0b0100, bk = 0b0010, bq = 0b0001};  // castling
    extern const string piece_to_string;
    extern const unordered_map<char, int> string_to_piece;
    const int max_moves = 218;
}

namespace random_numbers
{
    unsigned int random_32_bit_number();
    U64 random_64_bit_number();
    U64 random_magic_number();
}

namespace board_utils
{
    struct board_state {
        U64 bitboards[12];
        int castle;  // KQkq
        int enpassant;
        int side;
        U64 occupancies[3];
        // TODO: half and full move counters
    };

    board_state parse_fen(std::string fen);

    void print_board(board_state board);
}

#endif // utils
