#include "MoveGenerator/MoveGenerator.h"
#include "MoveGenerator/AttackTables.h"
#include "utils.h"
#include <iostream>
#include <array>
#include <span>
#include <format>
using std::format;
using std::cout;
using std::endl;
using std::span;
using std::array;

using piece_attacks::pawn_attacks;
using piece_attacks::knight_attacks;
using piece_attacks::bishop_attacks;
using piece_attacks::rook_attacks;
using piece_attacks::queen_attacks;
using piece_attacks::king_attacks;
using board_utils::board_state;
using namespace constants;
using namespace bitboard_utils;


namespace move_generator
{
    bool is_square_attacked(int square, board_state &board)
    {
        int side = (board.side == white) ? black : white;  // enemy of the current side to move
        if ((side == white) && (pawn_attacks(square, black) & board.bitboards[P])) return true;
        if ((side == black) && (pawn_attacks(square, white) & board.bitboards[p])) return true;
        if (knight_attacks(square) & ((side == white) ? board.bitboards[N] : board.bitboards[n])) return true;
        if (bishop_attacks(square, board.occupancies[both]) & ((side == white) ? board.bitboards[B] : board.bitboards[b])) return true;
        if (rook_attacks(square, board.occupancies[both]) & ((side == white) ? board.bitboards[R] : board.bitboards[r])) return true;
        if (queen_attacks(square, board.occupancies[both]) & ((side == white) ? board.bitboards[Q] : board.bitboards[q])) return true;
        if (king_attacks(square) & ((side == white) ? board.bitboards[K] : board.bitboards[k])) return true;
        return false;
    }

    void print_attacked(board_state &board)
    {
        for (int rank = 0; rank < 8; rank++)
        {
            cout << 8 - rank << "   ";
            for (int file = 0; file < 8; file++)
            {
                int square = 8 * rank + file;
                int bit = is_square_attacked(square, board);
                cout << bit << " ";
            }
            cout << endl;
        }
        cout << endl;
        cout << "    " << "a b c d e f g h" << endl << endl << endl;
    }

    void print_move_list(span<unsigned int> moves)
    {
        cout << "Move      Piece      Capture      En passant      Castle" << endl;
        for (int i = 0; i < moves.size(); i++)
        {
            unsigned int move = moves[i];
            cout << square_to_coordinates[move_source(move)] << square_to_coordinates[move_target(move)];
            cout << promotion_to_string[move_promotion(move)] << "     " << piece_to_string[move_piece(move)];
            cout << "          " << move_capture(move) << "            ";
            cout << move_enpassant(move) << "               " << move_castle(move) << endl;
        }

        cout << endl << "Number of moves:        " << moves.size() << endl << endl;
    }

    span<unsigned int> generate_moves(board_state &board, array<unsigned int, max_moves> _moves, bool captures_only)
    {
        span<unsigned int> moves_list(_moves);
        int move_index = 0;
        _generate_king_moves(board, captures_only, moves_list, move_index);
        _generate_pawn_moves(board, captures_only, moves_list, move_index);
        _generate_knight_moves(board, captures_only, moves_list, move_index);
        _generate_bishop_moves(board, captures_only, moves_list, move_index);
        _generate_rook_moves(board, captures_only, moves_list, move_index);
        _generate_queen_moves(board, captures_only, moves_list, move_index);

        span<unsigned int> moves = moves_list.subspan(0, move_index);
        return moves;
    }

    void _generate_pawn_moves(board_state &board, bool captures_only, span<unsigned int> moves, int &move_index)
    {
        if (board.side == white)
        {
            int piece = P;
            U64 bitboard = board.bitboards[piece];
            while (bitboard)
            {
                int source = least_significant_bit_index(bitboard);
                int target = source - 8;
                if (!(target < a8) && !get_bit(board.occupancies[both], target))
                {
                    if (!captures_only)
                    {
                        if (target >= a8 && target <= h8)
                        {
                            // generate promotions
                            moves[move_index++] = encode_move(source, target, piece, 0b1000, 0, 0, 0);  // queen
                            moves[move_index++] = encode_move(source, target, piece, 0b0100, 0, 0, 0);  // rook
                            moves[move_index++] = encode_move(source, target, piece, 0b0010, 0, 0, 0);  // bishop
                            moves[move_index++] = encode_move(source, target, piece, 0b0001, 0, 0, 0);  // knight
                        }
                        else
                        {
                            // pawn push
                            moves[move_index++] = encode_move(source, target, piece, 0b0000, 0, 0, 0);
                            if ((source >= a2 && source <= h2) && !get_bit(board.occupancies[both], target - 8))
                            {
                                // double push
                                moves[move_index++] = encode_move(source, target - 8, piece, 0b0000, 0, 0, 0);
                            }
                        }
                    }
                }

                U64 attacksboard = pawn_attacks(source, board.side) & board.occupancies[black];
                while (attacksboard)
                {
                    target = least_significant_bit_index(attacksboard);
                    
                    if (target >= a8 && target <= h8)
                    {
                        // generate capture promotions
                        moves[move_index++] = encode_move(source, target, piece, 0b1000, 1, 0, 0);  // queen
                        moves[move_index++] = encode_move(source, target, piece, 0b0100, 1, 0, 0);  // rook
                        moves[move_index++] = encode_move(source, target, piece, 0b0010, 1, 0, 0);  // bishop
                        moves[move_index++] = encode_move(source, target, piece, 0b0001, 1, 0, 0);  // knight
                    }
                    else
                        // normal capture
                        moves[move_index++] = encode_move(source, target, piece, 0b0000, 1, 0, 0);
                    pop_bit(attacksboard, target);
                }                        
                if (board.enpassant != no_square)
                {
                    U64 enpassant_attack = pawn_attacks(source, board.side) & (1ULL << board.enpassant);
                    if (enpassant_attack)
                    {
                        // en passant
                        moves[move_index++] = encode_move(source, board.enpassant, piece, 0b0000, 1, 1, 0);
                    }
                }
                
                // all pawn moves generated from this position
                pop_bit(bitboard, source);
            }
        }
        else if (board.side == black)
        {
            int piece = p;
            U64 bitboard = board.bitboards[piece];
            while (bitboard)
            {
                int source = least_significant_bit_index(bitboard);
                int target = source + 8;
                if (!(target > h1) && !get_bit(board.occupancies[both], target))
                {
                    if (!captures_only)
                    {
                        if (target >= a1 && target <= h1)
                        {
                            // generate promotions
                            moves[move_index++] = encode_move(source, target, piece, 0b1000, 0, 0, 0);  // queen
                            moves[move_index++] = encode_move(source, target, piece, 0b0100, 0, 0, 0);  // rook
                            moves[move_index++] = encode_move(source, target, piece, 0b0010, 0, 0, 0);  // bishop
                            moves[move_index++] = encode_move(source, target, piece, 0b0001, 0, 0, 0);  // knight
                        }
                        else
                        {
                            // pawn push
                            moves[move_index++] = encode_move(source, target, piece, 0b0000, 0, 0, 0);
                            if ((target >= a1 && target <= h1) && !get_bit(board.occupancies[both], target + 8))
                            {
                                // double push
                                moves[move_index++] = encode_move(source, target + 8, piece, 0b0000, 0, 0, 0);
                            }
                        }
                    }
                }

                U64 attacksboard = pawn_attacks(source, board.side) & board.occupancies[white];
                while (attacksboard)
                {
                    target = least_significant_bit_index(attacksboard);
                    
                    if (target >= a1 && target <= h1)
                    {
                        // generate capture promotions
                        moves[move_index++] = encode_move(source, target, piece, 0b1000, 1, 0, 0);  // queen
                        moves[move_index++] = encode_move(source, target, piece, 0b0100, 1, 0, 0);  // rook
                        moves[move_index++] = encode_move(source, target, piece, 0b0010, 1, 0, 0);  // bishop
                        moves[move_index++] = encode_move(source, target, piece, 0b0001, 1, 0, 0);  // knight
                    }
                    else
                        // normal capture
                        moves[move_index++] = encode_move(source, target, piece, 0b0000, 1, 0, 0);
                    pop_bit(attacksboard, target);
                }                        
                if (board.enpassant != no_square)
                {
                    U64 enpassant_attack = pawn_attacks(source, board.side) & (1ULL << board.enpassant);
                    if (enpassant_attack)
                    {
                        // en passant
                        moves[move_index++] = encode_move(source, board.enpassant, piece, 0b0000, 1, 1, 0);
                    }
                }
                
                // all pawn moves generated from this position
                pop_bit(bitboard, source);
            }
        }
    }

    void _generate_king_moves(board_state &board, bool captures_only, span<unsigned int> moves, int &move_index)
    {
        int piece = (board.side == white) ? K : k;
        int enemy_color = (board.side == white) ? black : white;
        if (board.side == white && !captures_only)
        {
            // white kingside castle
            int castling_available = board.castle & wk;
            int no_pieces_between = !get_bit(board.occupancies[both], f1) && !get_bit(board.occupancies[both], g1);
            int king_not_through_check = !is_square_attacked(e1, board) && !is_square_attacked(f1, board) && !is_square_attacked(g1, board);
            if (castling_available && no_pieces_between && king_not_through_check)
            {
                moves[move_index++] = encode_move(e1, g1, piece, 0b0000, 0, 0, 1);
            }

            // white queenside castle
            castling_available = board.castle & wq;
            no_pieces_between = !get_bit(board.occupancies[both], d1) && !get_bit(board.occupancies[both], c1) && !get_bit(board.occupancies[both], b1);
            king_not_through_check = !is_square_attacked(e1, board) && !is_square_attacked(d1, board) && !is_square_attacked(c1, board);
            if (castling_available && no_pieces_between && king_not_through_check)
            {
                moves[move_index++] = encode_move(e1, c1, piece, 0b0000, 0, 0, 1);
            }
        }
        else if (board.side == black && !captures_only)
        {
            // black kingside castle
            int castling_available = board.castle & bk;
            int no_pieces_between = !get_bit(board.occupancies[both], f8) && !get_bit(board.occupancies[both], g8);
            int king_not_through_check = !is_square_attacked(e8, board) && !is_square_attacked(f8, board) && !is_square_attacked(g8, board);
            if (castling_available && no_pieces_between && king_not_through_check)
            {
                moves[move_index++] = encode_move(e8, g8, piece, 0b0000, 0, 0, 1);
            }

            // black queenside castle
            castling_available = board.castle & bq;
            no_pieces_between = !get_bit(board.occupancies[both], d8) && !get_bit(board.occupancies[both], c8) && !get_bit(board.occupancies[both], b8);
            king_not_through_check = !is_square_attacked(e8, board) && !is_square_attacked(d8, board) && !is_square_attacked(c8, board);
            if (castling_available && no_pieces_between && king_not_through_check)
            {
                moves[move_index++] = encode_move(e8, c8, piece, 0b0000, 0, 0, 1);
            }
        }

        // normal king moves
        U64 bitboard = board.bitboards[piece];
        while (bitboard)
        {
            int source = least_significant_bit_index(bitboard);
            U64 attacksboard = king_attacks(source) & ~board.occupancies[board.side];
            if (captures_only)
                attacksboard &= board.occupancies[enemy_color];

            while (attacksboard)
            {
                int target = least_significant_bit_index(attacksboard);
                if (!get_bit(board.occupancies[enemy_color], target))
                    // non-capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 0, 0, 0);
                else
                    // capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 1, 0, 0);
                pop_bit(attacksboard, target);
            }
            pop_bit(bitboard, source);
        }
    }

    void _generate_knight_moves(board_state &board, bool captures_only, span<unsigned int> moves, int &move_index)
    {
        int piece = (board.side == white) ? N : n;
        int enemy_color = (board.side == white) ? black : white;
        U64 bitboard = board.bitboards[piece];
        while (bitboard)
        {
            int source = least_significant_bit_index(bitboard);
            U64 attacksboard = knight_attacks(source) & ~board.occupancies[board.side];
            if (captures_only)
                attacksboard &= board.occupancies[enemy_color];

            while (attacksboard)
            {
                int target = least_significant_bit_index(attacksboard);
                if (!get_bit(board.occupancies[enemy_color], target))
                    // non-capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 0, 0, 0);
                else
                    // capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 1, 0, 0);
                pop_bit(attacksboard, target);
            }
            pop_bit(bitboard, source);
        }
    }

    void _generate_bishop_moves(board_state &board, bool captures_only, span<unsigned int> moves, int &move_index)
    {
        int piece = (board.side == white) ? B : b;
        int enemy_color = (board.side == white) ? black : white;
        U64 bitboard = board.bitboards[piece];
        while (bitboard)
        {
            int source = least_significant_bit_index(bitboard);
            U64 attacksboard = bishop_attacks(source, board.occupancies[both]) & ~board.occupancies[board.side];
            if (captures_only)
                attacksboard &= board.occupancies[enemy_color];

            while (attacksboard)
            {
                int target = least_significant_bit_index(attacksboard);
                if (!get_bit(board.occupancies[enemy_color], target))
                    // non-capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 0, 0, 0);
                else
                    // capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 1, 0, 0);
                pop_bit(attacksboard, target);
            }
            pop_bit(bitboard, source);
        }
    }

    void _generate_rook_moves(board_state &board, bool captures_only, span<unsigned int> moves, int &move_index)
    {
        int piece = (board.side == white) ? R : r;
        int enemy_color = (board.side == white) ? black : white;
        U64 bitboard = board.bitboards[piece];
        while (bitboard)
        {
            int source = least_significant_bit_index(bitboard);
            U64 attacksboard = rook_attacks(source, board.occupancies[both]) & ~board.occupancies[board.side];
            if (captures_only)
                attacksboard &= board.occupancies[enemy_color];

            while (attacksboard)
            {
                int target = least_significant_bit_index(attacksboard);
                if (!get_bit(board.occupancies[enemy_color], target))
                    // non-capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 0, 0, 0);
                else
                    // capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 1, 0, 0);
                pop_bit(attacksboard, target);
            }
            pop_bit(bitboard, source);
        }
    }

    void _generate_queen_moves(board_state &board, bool captures_only, span<unsigned int> moves, int &move_index)
    {
        int piece = (board.side == white) ? Q : q;
        int enemy_color = (board.side == white) ? black : white;
        U64 bitboard = board.bitboards[piece];
        while (bitboard)
        {
            int source = least_significant_bit_index(bitboard);
            U64 attacksboard = (bishop_attacks(source, board.occupancies[both]) | rook_attacks(source, board.occupancies[both])) & ~board.occupancies[board.side];
            if (captures_only)
                attacksboard &= board.occupancies[enemy_color];

            while (attacksboard)
            {
                int target = least_significant_bit_index(attacksboard);
                if (!get_bit(board.occupancies[enemy_color], target))
                    // non-capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 0, 0, 0);
                else
                    // capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 1, 0, 0);
                pop_bit(attacksboard, target);
            }
            pop_bit(bitboard, source);
        }
    }


    unsigned int encode_move(int source, int target, int piece, int promotion, bool capture, bool enpassant, bool castle)
    {
        unsigned int move = source | (target << 6) | (piece << 12) | (promotion << 16) | (capture << 20) | (enpassant << 21) | (castle << 22);
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

    bool move_capture(unsigned int move)
    {
        return (move >> 20) & 1;
    }

    bool move_enpassant(unsigned int move)
    {
        return (move >> 21) & 1;
    }

    bool move_castle(unsigned int move)
    {
        return (move >> 22) & 1;
    }
}
