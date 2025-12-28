#ifndef board_representation
#define board_representation

#include <string>
#include <span>
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
    int find_piece(board_state &board, int square);
    bool is_promoting(board_state &board);
    unsigned int encode_move(int source, int target, int piece, int promotion, int capture, bool double_push, bool enpassant, bool castle);
    unsigned int encode_move(board_state &board, string move);
    int move_source(unsigned int  move);
    int move_target(unsigned int  move);
    int move_piece(unsigned int  move);
    int move_promotion(unsigned int  move);
    int move_capture(unsigned int  move);
    bool move_double_push(unsigned int move);
    bool move_enpassant(unsigned int  move);
    bool move_castle(unsigned int  move);
    string move_to_string(unsigned int move);
    bool is_promotion(int piece, int target);
}

namespace board_utils
{
    board::board_state parse_fen(string fen);
    U64 get_zobrist_hash(board::board_state &board);
    void print_board(board::board_state &board);
    void print_move_list(std::span<unsigned int> move_list);
    int string_to_square(const string& square);
}

#endif
