#ifndef moveGenerator
#define moveGenerator

#include "utils.h"
#include "Board/board.h"
#include <array>
#include <span>
#include <string>

using std::span;
using std::array;
using std::string;
using board::board_state;
using namespace constants;


namespace move_generator
{
    bool is_square_attacked(int square, board_state &board);
    bool in_check_after_en_passant(board_state &board, int source, int enemy_pawn_location);
    void print_attacked(board_state &board);
    void print_move_list(span<unsigned int> moves);
    int perft(board_state board, int depth);
    void perft_debug(board_state board, int depth);
    void perft_test_all_moves();

    struct king_info {
        int n_checks = 0;
        U64 check_rays = 0ULL;
        U64 pin_rays = 0ULL;
    };
    king_info _find_check_and_pin_masks(board_state &board);

    span<unsigned int> generate_moves(board_state &board, array<unsigned int, 218> &move_list, bool no_quiet_moves);
    void _generate_pawn_moves(board_state &board, king_info &info, bool no_quiet_moves, span<unsigned int> moves, int &move_index);
    void _generate_king_moves(board_state &board, king_info &info, bool no_quiet_moves, span<unsigned int> moves, int &move_index);
    void _generate_knight_moves(board_state &board, king_info &info, bool no_quiet_moves, span<unsigned int> moves, int &move_index);
    void _generate_slider_moves(board_state &board, king_info &info, bool no_quiet_moves, span<unsigned int> moves, int &move_index);
}

#endif  // moveGenerator
