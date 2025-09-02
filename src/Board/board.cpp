#include <iostream>
#include "utils.h"
#include "Board/board.h"
#include "MoveGenerator/MoveGenerator.h"
#include "Engine/transpositionTable.h"


using namespace bitboard_utils;
using namespace constants;
using namespace zobrist;

using std::cout;
using std::endl;
using board::board_state;


namespace board
{
    board_state make_move(board_state board, unsigned int move)
    {
        // remove old piece location
        int source = move_source(move);
        int target = move_target(move);
        int piece = move_piece(move);
        int enemy = board.side == white ? black : white;
        pop_bit(board.bitboards[piece], source);
        pop_bit(board.occupancies[board.side], source);
        set_bit(board.occupancies[board.side], target);
        board.zobrist_hash ^= zobrist_pieces[piece][source];

        // update castling rights
        board.zobrist_hash ^= zobrist_castle[board.castle];
        if (piece == R)
        {
            if (source == h1) board.castle &= ~wk;
            else if (source == a1) board.castle &= ~wq;
        }
        else if (piece == r)
        {
            if (source == h8) board.castle &= ~bk;
            else if (source == a8) board.castle &= ~bq;
        }
        if (piece == K) board.castle &= 0b0011;
        else if (piece == k) board.castle &= 0b1100;
        board.zobrist_hash ^= zobrist_castle[board.castle];

        // set piece at new location considering possible promotion
        int promotion = move_promotion(move);
        if (promotion == no_promotion)
        {
            set_bit(board.bitboards[piece], target);
            board.zobrist_hash ^= zobrist_pieces[piece][target];
        }
        else
        {
            if (promotion == promotion_queen)
            {
                set_bit(board.side == white ? board.bitboards[Q] : board.bitboards[q], target);
                board.zobrist_hash ^= zobrist_pieces[board.side == white ? Q : q][target];
            }
            else if (promotion == promotion_rook)
            {
                set_bit(board.side == white ? board.bitboards[R] : board.bitboards[r], target);
                board.zobrist_hash ^= zobrist_pieces[board.side == white ? R : r][target];
            }
            else if (promotion == promotion_bishop)
            {
                set_bit(board.side == white ? board.bitboards[B] : board.bitboards[b], target);
                board.zobrist_hash ^= zobrist_pieces[board.side == white ? B : b][target];
            }
            else if (promotion == promotion_knight)
            {
                set_bit(board.side == white ? board.bitboards[N] : board.bitboards[n], target);
                board.zobrist_hash ^= zobrist_pieces[board.side == white ? N : n][target];
            }
        }

        // if move was a capture, remove captured piece considering en passant
        int taken_piece = move_capture(move);
        if (taken_piece != no_piece)
        {
            int piece_location = move_enpassant(move) ? (board.side == white ? target + 8 : target - 8) : target;
            pop_bit(board.occupancies[enemy], piece_location);
            pop_bit(board.bitboards[taken_piece], piece_location);
            board.zobrist_hash ^= zobrist_pieces[taken_piece][piece_location];
        }

        // set en passant square if double push
        if (board.enpassant != no_square) board.zobrist_hash ^= zobrist_enpassant[board.enpassant];
        if (move_double_push(move))
        {
            board.enpassant = board.side == white ? target + 8 : target - 8;
            board.zobrist_hash ^= zobrist_enpassant[board.enpassant];
        }
        else
        {
            board.enpassant = no_square;
        }

        // if move was a castle, move rook
        if (move_castle(move))
        {
            if (board.side == white)
            {
                if (target == g1)
                {
                    pop_bit(board.bitboards[R], h1);
                    pop_bit(board.occupancies[board.side], h1);
                    set_bit(board.bitboards[R], f1);
                    set_bit(board.occupancies[board.side], f1);
                    board.zobrist_hash ^= zobrist_pieces[R][h1];
                    board.zobrist_hash ^= zobrist_pieces[R][f1];
                }
                else if (target == c1)
                {
                    pop_bit(board.bitboards[R], a1);
                    pop_bit(board.occupancies[board.side], a1);
                    set_bit(board.bitboards[R], d1);
                    set_bit(board.occupancies[board.side], d1);
                    board.zobrist_hash ^= zobrist_pieces[R][a1];
                    board.zobrist_hash ^= zobrist_pieces[R][d1];
                }
            }
            else
            {
                if (target == g8)
                {
                    pop_bit(board.bitboards[r], h8);
                    pop_bit(board.occupancies[board.side], h8);
                    set_bit(board.bitboards[r], f8);
                    set_bit(board.occupancies[board.side], f8);
                    board.zobrist_hash ^= zobrist_pieces[r][h8];
                    board.zobrist_hash ^= zobrist_pieces[r][f8];
                }
                else if (target == c8)
                {
                    pop_bit(board.bitboards[r], a8);
                    pop_bit(board.occupancies[board.side], a8);
                    set_bit(board.bitboards[r], d8);
                    set_bit(board.occupancies[board.side], d8);
                    board.zobrist_hash ^= zobrist_pieces[r][a8];
                    board.zobrist_hash ^= zobrist_pieces[r][d8];
                }
            }
        }

        board.occupancies[both] = board.occupancies[white] | board.occupancies[black];

        // update the player to move
        board.side = board.side == white ? black : white;
        board.zobrist_hash ^= zobrist_side;

        return board;
    }

    int find_captured_piece(board_state &board, int square)
    {
        int pawn = board.side == white ? 6 : 0;
        int king = board.side == white ? 11 : 5;
        for (int taken_piece = pawn; taken_piece <= king; taken_piece++)
        {
            if (get_bit(board.bitboards[taken_piece], square))
            {
                return taken_piece;
            }
        }
        return no_piece;
    }

    unsigned int encode_move(int source, int target, int piece, int promotion, int capture, bool double_push, bool enpassant, bool castle)
    {
        unsigned int move = source | (target << 6) | (piece << 12) | (promotion << 16) | (capture << 20) | (double_push << 24) | (enpassant << 25) | (castle << 26);
        return move;
    }
    
    int move_source(unsigned int move)
    {
        return move & 0b111111;
    }

    int move_target(unsigned int move)
    {
        return (move >> 6) & 0b111111;
    }

    int move_piece(unsigned int move)
    {
        return (move >> 12) & 0b1111;
    }

    int move_promotion(unsigned int move)
    {
        return (move >> 16) & 0b1111;
    }

    int move_capture(unsigned int move)
    {
        return (move >> 20) & 0b1111;
    }

    bool move_double_push(unsigned int move)
    {
        return (move >> 24) & 1;
    }

    bool move_enpassant(unsigned int move)
    {
        return (move >> 25) & 1;
    }

    bool move_castle(unsigned int move)
    {
        return (move >> 26) & 1;
    }

    string move_to_string(unsigned int move)
    {
        return square_to_coordinates[move_source(move)] + square_to_coordinates[move_target(move)] + promotion_to_string[move_promotion(move)];
    }
}

namespace board_utils
{
    board_state parse_fen(string fen)
    {
        int fen_index = 0;
        board_state state = board_state{};
        
        // update bitboards
        int square = 0;
        while (square < 64)
        {
            if ((fen[fen_index] >= 'a' && fen[fen_index] <= 'z') || (fen[fen_index] >= 'A' && fen[fen_index] <= 'Z'))
            {
                char piece_char = fen[fen_index];
                int piece = string_to_piece.at(piece_char);
                set_bit(state.bitboards[piece], square);
                square++;
            }
            else if (fen[fen_index] >= '0' && fen[fen_index] <= '9')
            {
                int offset = fen[fen_index] - '0';
                int current_piece = -1;
                for (int piece = P; piece <= k; piece++)
                {
                    if (get_bit(state.bitboards[piece], square))
                        current_piece = piece;
                }

                square += offset;
            }
            fen_index++;
        }
        fen_index++;

        // update side to move
        (fen[fen_index] == 'w') ? (state.side = white) : (state.side = black);
        fen_index += 2;

        // update castling rights
        while (fen[fen_index] != ' ')
        {
            switch (fen[fen_index])
            {
                case 'K': state.castle |= wk; break;
                case 'Q': state.castle |= wq; break;
                case 'k': state.castle |= bk; break;
                case 'q': state.castle |= bq; break;
                case '-': break;
            }
            fen_index++;
        }
        fen_index++;

        // update the en passant square
        if (fen[fen_index] != '-')
        {
            int file = fen[fen_index] - 'a';
            int rank = 8 - (fen[fen_index + 1] - '0');

            state.enpassant = rank * 8 + file;
        }        
        else
            state.enpassant = no_square;

        // update occupancies
        for (int piece = P; piece <= K; piece++)
            state.occupancies[white] |= state.bitboards[piece];
        
        for (int piece = p; piece <= k; piece++)
            state.occupancies[black] |= state.bitboards[piece];
        state.occupancies[both] |= state.occupancies[white];
        state.occupancies[both] |= state.occupancies[black];

        // TODO: update the halfmove clock
        // TODO: update the fullmove counter

        // recalculate the zobrist hash
        state.zobrist_hash = get_zobrist_hash(state);
        return state;
    }

    U64 get_zobrist_hash(board_state &board)
    {
        U64 zobrist_hash = 0ULL;
        for (int piece = P; piece <= k; piece++)
        {
            U64 bitboard = board.bitboards[piece];
            while (bitboard)
            {
                int square = least_significant_bit_index(bitboard);
                zobrist_hash ^= zobrist_pieces[piece][square];
                pop_bit(bitboard, square);
            }
        }
        zobrist_hash ^= zobrist_castle[board.castle];
        if (board.side == black) zobrist_hash ^= zobrist_side;
        if (board.enpassant != no_square) zobrist_hash ^= zobrist_enpassant[board.enpassant];
        return zobrist_hash;
    }

    void print_board(board_state &state)
    {
        cout << endl;
        for (int rank = 0; rank < 8; rank++)
        {
            cout << 8 - rank << "   ";
            for (int file = 0; file < 8; file++)
            {
                int square = rank * 8 + file;
                int current_piece = -1;
                for (int piece = P; piece <= k; piece++)
                {
                    if (get_bit(state.bitboards[piece], square))
                        current_piece = piece;
                }
                cout << ((current_piece == -1) ? '.' : piece_to_string[current_piece]) << ' ';
            }
            cout << endl;
        }
        cout << endl;
        cout << "    " << "a b c d e f g h" << endl << endl << endl;

        cout << "    Side:          " << (state.side ? "black" : "white") << endl;
        cout << "    Enpassant:     " << (state.enpassant != no_square ? square_to_coordinates[state.enpassant] : "-") << endl;
        cout << "    Castling:      " << ((state.castle & wk) ? 'K' : '-') << ((state.castle & wq) ? 'Q' : '-') << ((state.castle & bk) ? 'k' : '-') << ((state.castle & bq) ? 'q' : '-') << endl;
        cout << "    Hash key:      " << state.zobrist_hash << endl << endl << endl;
    }
}
