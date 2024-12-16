#ifndef moveGenerator
#define moveGenerator

#include "utils.h"
#include <array>
#include <span>

using std::span;
using std::array;
using board_utils::board_state;
using namespace constants;


namespace move_generator
{
    bool is_square_attacked(int square, board_state &board);
    void print_attacked(board_state &board);
    void print_move_list(span<unsigned int> moves);

    span<unsigned int> generate_moves(board_state &board, array<unsigned int, 218> _moves, bool captures_only);
    void _generate_pawn_moves(board_state &board, bool captures_only, span<unsigned int> moves, int &move_index);
    void _generate_king_moves(board_state &board, bool captures_only, span<unsigned int> moves, int &move_index);
    void _generate_knight_moves(board_state &board, bool captures_only, span<unsigned int> moves, int &move_index);
    void _generate_bishop_moves(board_state &board, bool captures_only, span<unsigned int> moves, int &move_index);
    void _generate_rook_moves(board_state &board, bool captures_only, span<unsigned int> moves, int &move_index);
    void _generate_queen_moves(board_state &board, bool captures_only, span<unsigned int> moves, int &move_index);

    unsigned int encode_move(int source, int target, int piece, int promotion, bool capture, bool enpassant, bool castle);
    int move_source(unsigned int  move);
    int move_target(unsigned int  move);
    int move_piece(unsigned int  move);
    int move_promotion(unsigned int  move);
    bool move_capture(unsigned int  move);
    bool move_enpassant(unsigned int  move);
    bool move_castle(unsigned int  move);
}

#endif  // moveGenerator
