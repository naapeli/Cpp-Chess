#include <emscripten.h>
#include <string>
#include <string.h>
#include <array>
#include <span>
#include <algorithm>
#include <iostream>

#include "utils.h"
#include "wasm_wrapper.h"
#include "Board/board.h"
#include "MoveGenerator/MoveGenerator.h"
#include "MoveGenerator/AttackTables.h"

using board::encode_move;
using board::make_move;
using board::move_to_string;
using board_utils::parse_fen;
using move_generator::generate_moves;
using piece_attacks::init_all;

using std::string;
using std::array;
using std::span;

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void init_engine()
    {
        init_all();
    }

    EMSCRIPTEN_KEEPALIVE
    const char* get_best_move(int time_milli_seconds) {
        int evaluation = wrapper_state::engine.iterative_search(wrapper_state::state, time_milli_seconds);
        unsigned int best_move = wrapper_state::engine.best_move();
        string move = move_to_string(best_move);

        static char buffer[6]; 
        strcpy(buffer, move.c_str());
        return buffer;  // maybe return the evaluation at some point using some best_move_and_evaluation struct or something
    }

    EMSCRIPTEN_KEEPALIVE
    int make_move(const char* move) {
        string cppmove = move;
        std::erase(cppmove, ' ');
        unsigned int encoded_move = encode_move(wrapper_state::state, cppmove);

        array<unsigned int, 218> move_list;
        span<unsigned int> moves = generate_moves(wrapper_state::state, move_list, false);
        bool legal_move = std::ranges::contains(moves, encoded_move);  // this way is in C++23 I believe but not in C++20
        // bool legal_move = std::ranges::find(moves, encoded_move) != moves.end();

        if (legal_move)
        {
            wrapper_state::state = make_move(wrapper_state::state, encoded_move);
            return 1;
        }
        // std::cout << cppmove << std::endl << std::endl;
        // board_utils::print_move_list(moves);

        return 0;
    }

    EMSCRIPTEN_KEEPALIVE
    void new_state(const char* fen) {
        string cppfen = fen;
        wrapper_state::state = parse_fen(cppfen);
        // board_utils::print_board(wrapper_state::state);
    }
}
