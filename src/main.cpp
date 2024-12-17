#include <iostream>
#include <string>
#include <array>
#include <span>

#include "utils.h"
#include "Board/board.h"
#include "MoveGenerator/AttackTables.h"
#include "MoveGenerator/MoveGenerator.h"

using std::string;
using std::array;
using std::span;
using std::cout;
using std::endl;

using namespace constants;
using move_generator::generate_moves;
using move_generator::print_move_list;
using move_generator::perft;
using move_generator::perft_debug;
using move_generator::encode_move;
using piece_attacks::init_all;

using piece_attacks::align_mask;

using board::board_state;
using board::make_move;
using board_utils::parse_fen;
using board_utils::print_board;
using bitboard_utils::print_bitboard;


int main()
{
    string empty_position = "8/8/8/8/8/8/8/8 w - - 0 1";
    string start_position = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    string perft_position_2 = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -";
    string perft_position_3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -";
    string perft_position_4 = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
    string perft_position_5 = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
    string perft_position_6 = "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";

    init_all();
    board_state state = parse_fen(perft_position_2);
    // state = make_move(state, encode_move(f2, f4, P, no_promotion, 0, 1, 0, 0));
    // state = make_move(state, encode_move(e7, e5, p, no_promotion, 0, 1, 0, 0));
    // state = make_move(state, encode_move(e1, f2, K, no_promotion, 0, 0, 0, 0));
    // state = make_move(state, encode_move(d8, f6, q, no_promotion, 0, 0, 0, 0));
    print_board(state);


    // array<unsigned int, max_moves> move_list;
    // span<unsigned int> moves = generate_moves(state, move_list, false);
    // print_move_list(moves);

    int depth = 5;
    perft_debug(state, depth, false);

    return 0;
}
