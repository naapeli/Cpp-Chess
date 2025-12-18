#ifndef zobrist_hashing
#define zobrist_hashing

#include "utils.h"
#include <array>

using random_numbers::random_64_bit_number;
using std::array;

namespace zobrist
{
    extern array<array<U64, 12>, 64> zobrist_pieces;
    extern U64 zobrist_side;
    extern array<U64, 16> zobrist_castle;
    extern array<U64, 64> zobrist_enpassant;
    
    void init_zobrist_keys();
}

namespace transposition_table
{
    enum { exact, lowerbound, upperbound };
    struct transposition_table_entry
    {
        U64 zobrist_hash;
        U64 best_move;
        int depth;
        int node_type;
        int evaluation;
    };
    constexpr size_t max_size_mb = 64;
    constexpr size_t bytes_per_mb = 1024 * 1024;
    constexpr size_t array_size = (max_size_mb * bytes_per_mb) / sizeof(transposition_table_entry);
    extern array<transposition_table_entry, array_size> shallow_tt_table;
    extern array<transposition_table_entry, array_size> deep_tt_table;

    void add_move_to_table(U64 zobrist_hash, U64 move, int depth, int node_type, int evaluation);
    int get_evaluation_from_table(U64 zobrist_hash, int depth, int alpha, int beta);
}


#endif
