#ifndef board_representation
#define board_representation

namespace board
{
    struct board_state {
        U64 bitboards[12];
        int castle;  // KQkq
        int enpassant;
        bool side;
        U64 occupancies[3];
        // TODO: half and full move counters
    };
    board_state make_move(board_state board, unsigned int move);
}

namespace board_utils
{
    board::board_state parse_fen(std::string fen);
    void print_board(board::board_state board);
}

#endif
