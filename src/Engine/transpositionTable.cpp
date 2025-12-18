#include "Engine/transpositionTable.h"
#include "utils.h"
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
    array<transposition_table_entry, array_size> deep_tt_table;
    array<transposition_table_entry, array_size> shallow_tt_table;

    void add_move_to_table(U64 zobrist_hash, U64 move, int depth, int node_type, int evaluation)
    {
        int index = zobrist_hash % array_size;
        // if applicable, store in both the shallow and deep transposition tables
        transposition_table_entry shallow_entry = shallow_tt_table[index];
        shallow_entry.zobrist_hash = zobrist_hash;
        shallow_entry.best_move = move;
        shallow_entry.depth = depth;
        shallow_entry.node_type = node_type;
        shallow_entry.evaluation = evaluation;
        if (deep_tt_table[index].depth <= depth)
        {
            transposition_table_entry deep_entry = deep_tt_table[index];
            deep_entry.zobrist_hash = zobrist_hash;
            deep_entry.best_move = move;
            deep_entry.depth = depth;
            deep_entry.node_type = node_type;
            deep_entry.evaluation = evaluation;
        }
    }

    int get_evaluation_from_table(U64 zobrist_hash, int depth, int alpha, int beta)
    {
        int index = zobrist_hash % array_size;
        transposition_table_entry entry = deep_tt_table[index];
        if (entry.zobrist_hash == zobrist_hash)
        {
            if (entry.depth >= depth)
            {
                if (entry.node_type == exact) return entry.evaluation;
                if (entry.node_type == lowerbound && entry.evaluation <= alpha) return alpha;
                if (entry.node_type == upperbound && entry.evaluation >= beta) return beta;
            }
        }

        entry = shallow_tt_table[index];
        if (entry.zobrist_hash == zobrist_hash)
        {
            if (entry.depth >= depth)
            {
                if (entry.node_type == exact) return entry.evaluation;
                if (entry.node_type == lowerbound && entry.evaluation <= alpha) return alpha;
                if (entry.node_type == upperbound && entry.evaluation >= beta) return beta;
            }
        }

        return invalid_evaluation;
    }
}
