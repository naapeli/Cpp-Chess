#include "Engine/transpositionTable.h"
#include "utils.h"

#include <iostream>
#include <array>

using namespace constants;
using random_numbers::random_64_bit_number;
using transposition_table::transposition_table_entry;

using std::array;


namespace zobrist
{
    array<array<U64, 12>, 64> zobrist_pieces;
    U64 zobrist_side;
    array<U64, 16> zobrist_castle;
    array<U64, 64> zobrist_enpassant;
    
    void init_zobrist_keys()
    {
        for (int piece = P; piece <= k; piece++)
        {
            for (int square = a8; square <= h1; square++)
            {
                zobrist_pieces[piece][square] = random_64_bit_number();
            }
        }
        
        for (int castle = 0; castle < 16; castle++)
        {
            zobrist_castle[castle] = random_64_bit_number();
        }

        for (int square = a8; square <= h1; square++)
        {
            zobrist_enpassant[square] = random_64_bit_number();
        }

        zobrist_side = random_64_bit_number();
    }
}

namespace transposition_table
{
    array<transposition_table_entry, array_size> tt_table;

    void add_move_to_table(U64 zobrist_hash, unsigned int move, int depth, int node_type, int evaluation)
    {
        int index = zobrist_hash % array_size;
        transposition_table_entry& old_entry = tt_table[index];
        if (old_entry.zobrist_hash == zobrist_hash && old_entry.depth >= depth) return;  // if we have stored a deeper evaluation, keep that, but otherwise store the entry
        
        transposition_table_entry& deep_entry = tt_table[index];
        deep_entry.zobrist_hash = zobrist_hash;
        deep_entry.best_move = move;
        deep_entry.depth = depth;
        deep_entry.node_type = node_type;
        deep_entry.evaluation = evaluation;
    }

    // int get_evaluation_from_table(U64 zobrist_hash, int depth, int alpha, int beta)
    // {
    //     int index = zobrist_hash % array_size;
    //     transposition_table_entry& entry = tt_table[index];
    //     if (entry.zobrist_hash == zobrist_hash)
    //     {
    //         if (entry.depth >= depth)
    //         {
    //             if (entry.node_type == exact) return entry.evaluation;
    //             if (entry.node_type == lowerbound && entry.evaluation <= alpha) return alpha;
    //             if (entry.node_type == upperbound && entry.evaluation >= beta) return beta;
    //         }
    //     }

    //     return invalid_evaluation;
    // }

    // unsigned int get_best_move_from_table(U64 zobrist_hash, int depth)
    // {
    //     int index = zobrist_hash % array_size;
    //     transposition_table_entry& entry = tt_table[index];
    //     if (entry.zobrist_hash == zobrist_hash && entry.depth >= depth)
    //     {
    //         return entry.best_move;
    //     }

    //     return invalid_move;
    // }
    transposition_table_entry& get_entry_from_table(U64 zobrist_hash)
    {
        int index = zobrist_hash % array_size;
        return tt_table[index];
    }
}
