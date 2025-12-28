#include <string>

#include "Board/board.h"
#include "Engine/engine.h"

using board_utils::parse_fen;


namespace wrapper_state
{
    string start_position = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    board::board_state state = parse_fen(start_position);

    Engine engine;
}
