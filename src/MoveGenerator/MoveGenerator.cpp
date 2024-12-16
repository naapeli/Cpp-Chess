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
using piece_attacks::align_mask;
using piece_attacks::not_a_file;
using piece_attacks::not_h_file;
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

    span<unsigned int> generate_moves(board_state &board, array<unsigned int, max_moves> _moves, bool no_quiet_moves)
    {
        king_info info = _find_check_and_pin_masks(board);

        span<unsigned int> moves_list(_moves);
        int move_index = 0;
        _generate_king_moves(board, info, no_quiet_moves, moves_list, move_index);

        // skip other moves as only king moves are possible in double check
        if (info.n_checks < 2)
        {
            _generate_pawn_moves(board, info, no_quiet_moves, moves_list, move_index);
            _generate_knight_moves(board, info, no_quiet_moves, moves_list, move_index);
            _generate_slider_moves(board, info, no_quiet_moves, moves_list, move_index);
            // _generate_bishop_moves(board, info, no_quiet_moves, moves_list, move_index);
            // _generate_rook_moves(board, info, no_quiet_moves, moves_list, move_index);
            // _generate_queen_moves(board, info, no_quiet_moves, moves_list, move_index);
        }

        span<unsigned int> moves = moves_list.subspan(0, move_index);
        return moves;
    }

    void _generate_pawn_moves(board_state &board, king_info &info, bool no_quiet_moves, span<unsigned int> moves, int &move_index)
    {
        int direction = board.side == white ? 1 : -1;
        int piece = board.side == white ? P : p;
        int enemy = board.side == white ? black : white;
        U64 bitboard = board.bitboards[piece];
        U64 empty_squares = ~board.occupancies[both];

        U64 push = (bitboard >> (8 * direction)) & empty_squares;
        U64 promotion_mask = board.side == white ? 0xFF : 0xFF00000000000000;
        U64 push_promotions = push & promotion_mask & info.check_rays;
        U64 push_no_promotion = push & ~promotion_mask & info.check_rays;
        U64 target_row = board.side == white ? 0xFF00000000 : 0xFF000000;
        U64 double_push_no_promotion = (push_no_promotion >> (8 * direction)) & empty_squares & target_row & info.check_rays;

        U64 edge_mask_1 = board.side == white ? not_h_file : not_a_file;
        U64 edge_mask_2 = board.side == white ? not_a_file : not_h_file;
        U64 capture_mask_1 = ((bitboard & edge_mask_1) >> (7 * direction)) & board.occupancies[enemy];
        U64 capture_mask_2 = ((bitboard & edge_mask_2) >> (9 * direction)) & board.occupancies[enemy];
        U64 capture_promotions_1 = capture_mask_1 & promotion_mask & info.check_rays;
        U64 capture_promotions_2 = capture_mask_2 & promotion_mask & info.check_rays;
        U64 capture_no_promotion_1 = capture_mask_1 & ~promotion_mask & info.check_rays;
        U64 capture_no_promotion_2 = capture_mask_2 & ~promotion_mask & info.check_rays;

        int king_location = least_significant_bit_index((board.side == white) ? board.bitboards[K] : board.bitboards[k]);

        // pushes
        if (!no_quiet_moves)
        {
            while (push_no_promotion)
            {
                int target = least_significant_bit_index(push_no_promotion);
                int source = target + 8 * direction;
                if (!get_bit(info.pin_rays, source) || (align_mask[source][king_location] == align_mask[target][king_location]))
                {
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 0, 0, 0);
                }
                pop_bit(push_no_promotion, target);
            }
            while (double_push_no_promotion)
            {
                int target = least_significant_bit_index(double_push_no_promotion);
                int source = target + 16 * direction;
                if (!get_bit(info.pin_rays, source) || (align_mask[source][king_location] == align_mask[target][king_location]))
                {
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 0, 0, 0);
                }
                pop_bit(double_push_no_promotion, target);
            }
        }

        // captures
        while (capture_no_promotion_1)
        {
            int target = least_significant_bit_index(capture_no_promotion_1);
            int source = target + 7 * direction;
            if (!get_bit(info.pin_rays, source) || (align_mask[source][king_location] == align_mask[target][king_location]))
            {
                moves[move_index++] = encode_move(source, target, piece, 0b0000, 1, 0, 0);
            }
            pop_bit(capture_no_promotion_1, target);
        }
        while (capture_no_promotion_2)
        {
            int target = least_significant_bit_index(capture_no_promotion_2);
            int source = target + 9 * direction;
            if (!get_bit(info.pin_rays, source) || (align_mask[source][king_location] == align_mask[target][king_location]))
            {
                moves[move_index++] = encode_move(source, target, piece, 0b0000, 1, 0, 0);
            }
            pop_bit(capture_no_promotion_2, target);
        }

        // promotions
        while (push_promotions)
        {
            int target = least_significant_bit_index(push_promotions);
            int source = target + 8 * direction;
            if (!get_bit(info.pin_rays, source) || (align_mask[source][king_location] == align_mask[target][king_location]))
            {
                moves[move_index++] = encode_move(source, target, piece, 0b1000, 0, 0, 0);  // queen
                moves[move_index++] = encode_move(source, target, piece, 0b0100, 0, 0, 0);  // rook
                moves[move_index++] = encode_move(source, target, piece, 0b0010, 0, 0, 0);  // bishop
                moves[move_index++] = encode_move(source, target, piece, 0b0001, 0, 0, 0);  // knight
            }
            pop_bit(push_promotions, target);
        }
        while (capture_promotions_1)
        {
            int target = least_significant_bit_index(capture_promotions_1);
            int source = target + 7 * direction;
            if (!get_bit(info.pin_rays, source) || (align_mask[source][king_location] == align_mask[target][king_location]))
            {
                moves[move_index++] = encode_move(source, target, piece, 0b1000, 0, 0, 0);  // queen
                moves[move_index++] = encode_move(source, target, piece, 0b0100, 0, 0, 0);  // rook
                moves[move_index++] = encode_move(source, target, piece, 0b0010, 0, 0, 0);  // bishop
                moves[move_index++] = encode_move(source, target, piece, 0b0001, 0, 0, 0);  // knight
            }
            pop_bit(capture_promotions_1, target);
        }
        while (capture_promotions_2)
        {
            int target = least_significant_bit_index(capture_promotions_2);
            int source = target + 9 * direction;
            if (!get_bit(info.pin_rays, source) || (align_mask[source][king_location] == align_mask[target][king_location]))
            {
                moves[move_index++] = encode_move(source, target, piece, 0b1000, 0, 0, 0);  // queen
                moves[move_index++] = encode_move(source, target, piece, 0b0100, 0, 0, 0);  // rook
                moves[move_index++] = encode_move(source, target, piece, 0b0010, 0, 0, 0);  // bishop
                moves[move_index++] = encode_move(source, target, piece, 0b0001, 0, 0, 0);  // knight
            }
            pop_bit(capture_promotions_2, target);
        }

        // en passant
        if (board.enpassant != no_square && (board.enpassant & info.check_rays))
        {
            int enemy = board.side == white ? black : white;
            U64 pawns_able_to_en_passant = pawn_attacks(board.enpassant, enemy) & board.bitboards[piece];
            while (pawns_able_to_en_passant)
            {
                int source = least_significant_bit_index(pawns_able_to_en_passant);
                if (!get_bit(info.pin_rays, source) || (align_mask[source][king_location] == align_mask[board.enpassant][king_location]))
                {
                    // TODO: Before adding to the move list, check that the king is not left in check after the en passant
                    moves[move_index++] = encode_move(source, board.enpassant, piece, 0b0000, 1, 1, 0);
                }
            }
        }
    }

    void _generate_king_moves(board_state &board, king_info &info, bool no_quiet_moves, span<unsigned int> moves, int &move_index)
    {
        int piece = (board.side == white) ? K : k;
        int enemy_color = (board.side == white) ? black : white;
        if (board.side == white && !no_quiet_moves)
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
        else if (board.side == black && !no_quiet_moves)
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
            U64 attacks_board = king_attacks(source) & ~board.occupancies[board.side];
            if (no_quiet_moves)
                attacks_board &= board.occupancies[enemy_color];

            while (attacks_board)
            {
                int target = least_significant_bit_index(attacks_board);
                if (!get_bit(board.occupancies[enemy_color], target))
                    // non-capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 0, 0, 0);
                else
                    // capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 1, 0, 0);
                pop_bit(attacks_board, target);
            }
            pop_bit(bitboard, source);
        }
    }

    void _generate_knight_moves(board_state &board, king_info &info, bool no_quiet_moves, span<unsigned int> moves, int &move_index)
    {
        int piece = (board.side == white) ? N : n;
        int enemy_color = (board.side == white) ? black : white;
        U64 bitboard = board.bitboards[piece] & ~info.pin_rays;
        U64 move_mask = ~board.occupancies[board.side] & info.check_rays;
        if (no_quiet_moves)
            move_mask &= board.occupancies[enemy_color];
        while (bitboard)
        {
            int source = least_significant_bit_index(bitboard);
            U64 attacks_board = knight_attacks(source) & move_mask;

            while (attacks_board)
            {
                int target = least_significant_bit_index(attacks_board);
                if (!get_bit(board.occupancies[enemy_color], target))
                    // non-capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 0, 0, 0);
                else
                    // capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 1, 0, 0);
                pop_bit(attacks_board, target);
            }
            pop_bit(bitboard, source);
        }
    }

    void _generate_slider_moves(board_state &board, king_info &info, bool no_quiet_moves, span<unsigned int> moves, int &move_index)
    {
        int enemy_color = (board.side == white) ? black : white;
        int king_location = least_significant_bit_index((board.side == white) ? board.bitboards[K] : board.bitboards[k]);
        U64 move_mask = ~board.occupancies[board.side] & info.check_rays;
        if (no_quiet_moves)
            move_mask &= board.occupancies[enemy_color];
        U64 orthogonal_bitboard = (board.side == white) ? board.bitboards[Q] | board.bitboards[R] : board.bitboards[q] | board.bitboards[r];
        U64 diagonal_bitboard = (board.side == white) ? board.bitboards[Q] | board.bitboards[B] : board.bitboards[q] | board.bitboards[b];
        if (info.n_checks)
        {
            orthogonal_bitboard &= ~info.pin_rays;
            diagonal_bitboard &= ~info.pin_rays;
        }
        while (orthogonal_bitboard)
        {
            int source = least_significant_bit_index(orthogonal_bitboard);
            int piece;
            if (board.side == white)
            {
                piece = get_bit(board.bitboards[Q], source) ? Q : R;
            }
            else
            {
                piece = get_bit(board.bitboards[q], source) ? q : r;
            }
            U64 attacks_board = rook_attacks(source, board.occupancies[both]) & move_mask;
            if (get_bit(info.pin_rays, source))
            {
                attacks_board &= align_mask[source][king_location];
            }
            while (attacks_board)
            {
                int target = least_significant_bit_index(attacks_board);
                if (get_bit(board.occupancies[enemy_color], target))
                    // capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 1, 0, 0);
                else
                    // non-capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 0, 0, 0);
                pop_bit(attacks_board, target);
            }
            pop_bit(orthogonal_bitboard, source);
        }
        while (diagonal_bitboard)
        {
            int source = least_significant_bit_index(diagonal_bitboard);
            int piece;
            if (board.side == white)
            {
                piece = get_bit(board.bitboards[Q], source) ? Q : B;
            }
            else
            {
                piece = get_bit(board.bitboards[q], source) ? q : b;
            }
            U64 attacks_board = bishop_attacks(source, board.occupancies[both]) & move_mask;
            if (get_bit(info.pin_rays, source))
            {
                attacks_board &= align_mask[source][king_location];
            }
            while (attacks_board)
            {
                int target = least_significant_bit_index(attacks_board);
                if (get_bit(board.occupancies[enemy_color], target))
                    // capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 1, 0, 0);
                else
                    // non-capture
                    moves[move_index++] = encode_move(source, target, piece, 0b0000, 0, 0, 0);
                pop_bit(attacks_board, target);
            }
            pop_bit(diagonal_bitboard, source);
        }
    }

    // void _generate_bishop_moves(board_state &board, king_info &info, bool no_quiet_moves, span<unsigned int> moves, int &move_index)
    // {
    //     int piece = (board.side == white) ? B : b;
    //     int enemy_color = (board.side == white) ? black : white;
    //     U64 bitboard = board.bitboards[piece];
    //     while (bitboard)
    //     {
    //         int source = least_significant_bit_index(bitboard);
    //         U64 attacks_board = bishop_attacks(source, board.occupancies[both]) & ~board.occupancies[board.side];
    //         if (no_quiet_moves)
    //             attacks_board &= board.occupancies[enemy_color];

    //         while (attacks_board)
    //         {
    //             int target = least_significant_bit_index(attacks_board);
    //             if (!get_bit(board.occupancies[enemy_color], target))
    //                 // non-capture
    //                 moves[move_index++] = encode_move(source, target, piece, 0b0000, 0, 0, 0);
    //             else
    //                 // capture
    //                 moves[move_index++] = encode_move(source, target, piece, 0b0000, 1, 0, 0);
    //             pop_bit(attacks_board, target);
    //         }
    //         pop_bit(bitboard, source);
    //     }
    // }

    // void _generate_rook_moves(board_state &board, king_info &info, bool no_quiet_moves, span<unsigned int> moves, int &move_index)
    // {
    //     int piece = (board.side == white) ? R : r;
    //     int enemy_color = (board.side == white) ? black : white;
    //     U64 bitboard = board.bitboards[piece];
    //     while (bitboard)
    //     {
    //         int source = least_significant_bit_index(bitboard);
    //         U64 attacks_board = rook_attacks(source, board.occupancies[both]) & ~board.occupancies[board.side];
    //         if (no_quiet_moves)
    //             attacks_board &= board.occupancies[enemy_color];

    //         while (attacks_board)
    //         {
    //             int target = least_significant_bit_index(attacks_board);
    //             if (!get_bit(board.occupancies[enemy_color], target))
    //                 // non-capture
    //                 moves[move_index++] = encode_move(source, target, piece, 0b0000, 0, 0, 0);
    //             else
    //                 // capture
    //                 moves[move_index++] = encode_move(source, target, piece, 0b0000, 1, 0, 0);
    //             pop_bit(attacks_board, target);
    //         }
    //         pop_bit(bitboard, source);
    //     }
    // }

    // void _generate_queen_moves(board_state &board, king_info &info, bool no_quiet_moves, span<unsigned int> moves, int &move_index)
    // {
    //     int piece = (board.side == white) ? Q : q;
    //     int enemy_color = (board.side == white) ? black : white;
    //     U64 bitboard = board.bitboards[piece];
    //     while (bitboard)
    //     {
    //         int source = least_significant_bit_index(bitboard);
    //         U64 attacks_board = (bishop_attacks(source, board.occupancies[both]) | rook_attacks(source, board.occupancies[both])) & ~board.occupancies[board.side];
    //         if (no_quiet_moves)
    //             attacks_board &= board.occupancies[enemy_color];

    //         while (attacks_board)
    //         {
    //             int target = least_significant_bit_index(attacks_board);
    //             if (!get_bit(board.occupancies[enemy_color], target))
    //                 // non-capture
    //                 moves[move_index++] = encode_move(source, target, piece, 0b0000, 0, 0, 0);
    //             else
    //                 // capture
    //                 moves[move_index++] = encode_move(source, target, piece, 0b0000, 1, 0, 0);
    //             pop_bit(attacks_board, target);
    //         }
    //         pop_bit(bitboard, source);
    //     }
    // }

    king_info _find_check_and_pin_masks(board_state &board)
    {
        king_info info = king_info{};

        int king = (board.side == white) ? K : k;
        int king_location = least_significant_bit_index(board.bitboards[king]);
        U64 own_pieces = (board.side == white) ? board.occupancies[white] : board.occupancies[black];
        U64 enemy_pieces = (board.side == white) ? board.occupancies[black] : board.occupancies[white];
        int king_row = king_location / 8;
        int king_file = king_location % 8;
        int r, f;
        int square;
        
        // check sliders for pins or checks
        array<array<int, 2>, 8> offsets = {{{1, 1}, {1, -1}, {-1, 1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
        for (int i = 0; i < offsets.size(); i++)
        {
            bool is_diagonal = i < 4;
            U64 possible_checkers;
            U64 ray_mask = 0ULL;
            if (board.side == white)
            {
                // if diagonals then bishop and queen else rook and queen
                possible_checkers = is_diagonal ? board.bitboards[b] : board.bitboards[r];
                possible_checkers |= board.bitboards[q];
            }
            else
            {
                // if diagonals then bishop and queen else rook and queen
                possible_checkers = is_diagonal ? board.bitboards[B] : board.bitboards[R];
                possible_checkers |= board.bitboards[Q];
            }
            if (count_bits(possible_checkers) == 0) continue;  // continue if no possible checking pieces in this direction

            int n_blockers = 0;
            int k = 1;
            r = king_row;
            f = king_file;
            // cout << "=======================" << endl;
            // cout << r << f << endl;
            while (king_row + k * offsets[i][0] >= 0 && king_row + k * offsets[i][0] <= 7 && king_file + k * offsets[i][1] >= 0 && king_file + k * offsets[i][1] <= 7)
            {
                r = king_row + k * offsets[i][0];
                f = king_file + k * offsets[i][1];
                // cout << r << f << endl;
                square = 8 * r + f;
                set_bit(ray_mask, square);
                if (get_bit(board.occupancies[both], square) == 0)  // no piece
                {
                    // if no piece in this square, continue
                    k++;
                    continue;
                }
                if (get_bit(possible_checkers, square) && n_blockers == 0)  // check
                {
                    info.n_checks++;
                    if (info.n_checks >= 2) return info;  // if in double check, we can return early as only king is able to move
                    info.check_rays |= ray_mask;
                    break;
                }
                if (get_bit(own_pieces, square))  // own piece
                {
                    n_blockers++;
                    if (n_blockers >= 2) break;
                    k++;
                    continue;
                }
                bool diagonal_pin = is_diagonal && (get_bit((board.side == white) ? (board.bitboards[q] | board.bitboards[b]) : (board.bitboards[Q] | board.bitboards[B]), square));
                bool orthogonal_pin = !is_diagonal && (get_bit((board.side == white) ? (board.bitboards[q] | board.bitboards[r]) : (board.bitboards[Q] | board.bitboards[R]), square));
                bool enemy_piece_with_blocker = get_bit(enemy_pieces, square) && n_blockers == 1;
                if (enemy_piece_with_blocker && (diagonal_pin || orthogonal_pin))  // enemy piece, which pins
                {
                    info.pin_rays |= ray_mask;
                    break;
                }
                break;  // enemy piece, which does not check or give a pin
            }
        }

        // knight checks
        U64 enemy_knights = (board.side == white) ? board.bitboards[n] : board.bitboards[N];
        U64 attacks_board = knight_attacks(king_location);
        while (attacks_board)
        {
            square = least_significant_bit_index(attacks_board);
            if (get_bit(enemy_knights, square))
            {
                info.n_checks++;
                if (info.n_checks >= 2) return info;
                set_bit(info.check_rays, square);
            }
            pop_bit(attacks_board, square);
        }

        // pawn checks
        U64 possible_checks;
        if (board.side == white)
        {
            attacks_board = pawn_attacks(king_location, white);
            possible_checks = board.bitboards[p] & attacks_board;
        }
        else
        {
            attacks_board = pawn_attacks(king_location, black);
            possible_checks = board.bitboards[P] & attacks_board;
        }
        while (possible_checks)
        {
            square = least_significant_bit_index(possible_checks);
            info.n_checks++;
            if (info.n_checks >= 2) return info;
            set_bit(info.check_rays, square);
            pop_bit(possible_checks, square);
        }

        // if no checks, all moves should be possible
        if (info.n_checks == 0)
        {
            info.check_rays = 0xFFFFFFFFFFFFFFFFULL;
        }

        return info;
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
