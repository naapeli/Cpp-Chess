#pragma once
#include "Board/board.h"
namespace evaluation {
int evaluate(const board::board_state& board);
bool insufficient_material(const board::board_state& board);
}
