#include "uci.h"
#include "Engine/evaluation.h"
#include "MoveGenerator/AttackTables.h"
#include "MoveGenerator/MoveGenerator.h"
#include <iostream>
#include <stdexcept>
#include <string>

// Batch diagnostics for the three pytest suites. One FEN per input line,
// one result per output line. No search is involved in --evaluate.
int main(int argc, char** argv) {
    if (argc == 1) return run_uci();
    try {
        const std::string mode = argv[1];
        int depth = 0;
        if (mode == "--perft" && argc == 3) {
            size_t end = 0;
            depth = std::stoi(argv[2], &end);
            if (end != std::string(argv[2]).size() || depth < 0)
                throw std::invalid_argument("perft depth must be nonnegative");
        } else if (argc != 2 || (mode != "--evaluate" && mode != "--legal-moves")) {
            throw std::invalid_argument("usage: engine [--evaluate | --legal-moves | --perft DEPTH]");
        }
        piece_attacks::init_all();
        std::string fen;
        while (std::getline(std::cin, fen)) {
            auto position = board_utils::parse_fen(fen);
            if (mode == "--evaluate") std::cout << evaluation::evaluate(position);
            else if (mode == "--perft") std::cout << move_generator::perft(position, depth);
            else {
                std::array<unsigned int, constants::max_moves> buffer;
                for (auto move : move_generator::generate_moves(position, buffer, false))
                    std::cout << board::move_to_string(move) << ' ';
            }
            std::cout << std::endl;
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
