#include <iostream>
#include <string>
#include "../include/utils.h"
#include "../include/MoveGenerator/AttackTables.h"

using namespace std;
using namespace constants;
using namespace board_utils;
using namespace bitboard_utils;
using namespace piece_attacks;
using namespace random_numbers;
using namespace magic_numbers;


int main()
{
    string start_position = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    string perft_position_2 = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -";
    string perft_position_3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -";
    string perft_position_4 = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
    string perft_position_5 = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
    string perft_position_6 = "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";

    board_state state = parse_fen(start_position);

    print_board(state);

    return 0;
}
