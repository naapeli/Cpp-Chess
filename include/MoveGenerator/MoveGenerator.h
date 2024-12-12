#ifndef moveGenerator
#define moveGenerator

#include "utils.h"
#include <vector>

using board_utils::board_state;
using namespace constants;


namespace move_generator
{
    bool is_square_attacked(int square, board_state board);
    std::vector<int> generate_moves(board_state board, bool captures_only);  // change to use #include <span> std::span as probably faster.
}

#endif  // moveGenerator