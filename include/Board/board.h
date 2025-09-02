#ifndef board_representation
#define board_representation

#include <string>
#include "utils.h"

using std::string;


namespace board
{
    struct board_state {
        U64 bitboards[12];
        int castle;  // KQkq
        int enpassant;
        bool side;
        U64 occupancies[3];
        U64 zobrist_hash;
        // TODO: half and full move counters
    };
    board_state make_move(board_state board, unsigned int move);

    int find_captured_piece(board_state &board, int square);
    unsigned int encode_move(int source, int target, int piece, int promotion, int capture, bool double_push, bool enpassant, bool castle);
    int move_source(unsigned int  move);
    int move_target(unsigned int  move);
    int move_piece(unsigned int  move);
    int move_promotion(unsigned int  move);
    int move_capture(unsigned int  move);
    bool move_double_push(unsigned int move);
    bool move_enpassant(unsigned int  move);
    bool move_castle(unsigned int  move);
    string move_to_string(unsigned int move);
}

namespace board_utils
{
    board::board_state parse_fen(std::string fen);
    U64 get_zobrist_hash(board::board_state &board);
    void print_board(board::board_state &board);
}

#endif
