#include "MoveGenerator/MoveGenerator.h"
#include "MoveGenerator/AttackTables.h"
#include "utils.h"
#include "Board/board.h"

#include <iostream>
#include <array>
#include <span>
#include <format>
#include <chrono>

using std::format;
using std::cout;
using std::endl;
using std::span;
using std::array;

using std::chrono::high_resolution_clock;

using piece_attacks::pawn_attacks;
using piece_attacks::knight_attacks;
using piece_attacks::bishop_attacks;
using piece_attacks::rook_attacks;
using piece_attacks::queen_attacks;
using piece_attacks::king_attacks;
using piece_attacks::align_mask;
using piece_attacks::not_a_file;
using piece_attacks::not_h_file;
using board::board_state;
using board::make_move;
using board_utils::print_board;
using board_utils::parse_fen;
using namespace constants;
using namespace bitboard_utils;

using board::encode_move;
using board::move_source;
using board::move_target;
using board::move_capture;
using board::move_promotion;
using board::move_enpassant;
using board::move_castle;
using board::move_piece;
using board::move_to_string;


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

    bool in_check_after_en_passant(board_state &board, int source, int enemy_pawn_location)
    {
        U64 enemy_orthogonal_sliders = board.side == white ? (board.bitboards[q] | board.bitboards[r]) : (board.bitboards[Q] | board.bitboards[R]);
        if (enemy_orthogonal_sliders)
        {
            U64 blockers = board.occupancies[both] ^ ((1ULL << source) | (1ULL << enemy_pawn_location) | (1ULL << board.enpassant));
            int king_location = least_significant_bit_index((board.side == white ? board.bitboards[K] : board.bitboards[k]));
            U64 attacks_board = rook_attacks(king_location, blockers);
            return (attacks_board & enemy_orthogonal_sliders) != 0;
        }
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
            cout << "          " << piece_to_string[move_capture(move)] << "            ";
            cout << move_enpassant(move) << "               " << move_castle(move) << endl;
        }

        cout << endl << "Number of moves:        " << moves.size() << endl << endl;
    }

    int perft(board_state board, int depth)
    {
        array<unsigned int, max_moves> move_list;
        span<unsigned int> moves = generate_moves(board, move_list, false);
        if (depth == 0)
        {
            return 1;
        }
        if (depth == 1)
        {
            return moves.size();
        }
        int n_moves = 0;
        for (int i = 0; i < moves.size(); i++)
        {
            unsigned int move = moves[i];
            board_state new_board = make_move(board, move);
            n_moves += perft(new_board, depth - 1);
        }
        return n_moves;
    }

    void perft_debug(board_state board, int depth)
    {
        array<unsigned int, max_moves> move_list;
        span<unsigned int> moves = generate_moves(board, move_list, false);
        cout << "Move     number of moves from position" << endl;
        int n_moves = 0;
        for (int i = 0; i < moves.size(); i++)
        {
            int move = moves[i];
            board_state new_board = make_move(board, move);
            int n_submoves = perft(new_board, depth - 1);
            n_moves += n_submoves;
            cout << move_to_string(move) << "    " << n_submoves << endl;
        }
        cout << "Total number of moves at depth " << depth << ": " << n_moves << endl;
    }

    void perft_test_all_moves()
    {
        string start_position = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        string perft_position_2 = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -";
        string perft_position_3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -";
        string perft_position_4 = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
        string perft_position_5 = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
        string perft_position_6 = "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";

        auto start = high_resolution_clock::now();

        board_state board = parse_fen(start_position);
        cout << "Generated moves: " << perft(board, 6) << " - True moves: " << 119060324 << endl;
        board = parse_fen(perft_position_2);
        cout << "Generated moves: " << perft(board, 5) << " - True moves: " << 193690690 << endl;
        board = parse_fen(perft_position_3);
        cout << "Generated moves: " << perft(board, 7) << " - True moves: " << 178633661 << endl;
        board = parse_fen(perft_position_4);
        cout << "Generated moves: " << perft(board, 6) << " - True moves: " << 706045033 << endl;
        board = parse_fen(perft_position_5);
        cout << "Generated moves: " << perft(board, 5) << " - True moves: " << 89941194 << endl;
        board = parse_fen(perft_position_6);
        cout << "Generated moves: " << perft(board, 5) << " - True moves: " << 164075551 << endl;

        auto stop = high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
        cout << "Time taken: " << duration.count() << " ms" << endl;
    }

    span<unsigned int> generate_moves(board_state &board, array<unsigned int, max_moves> &move_list, bool no_quiet_moves)
    {
        king_info info = _find_check_and_pin_masks(board);

        span<unsigned int> moves_list(move_list);
        int move_index = 0;
        _generate_king_moves(board, info, no_quiet_moves, moves_list, move_index);

        // skip other moves as only king moves are possible in double check
        if (info.n_checks < 2)
        {
            _generate_pawn_moves(board, info, no_quiet_moves, moves_list, move_index);
            _generate_knight_moves(board, info, no_quiet_moves, moves_list, move_index);
            _generate_slider_moves(board, info, no_quiet_moves, moves_list, move_index);
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

        U64 push = shift(bitboard, 8 * direction) & empty_squares;
        U64 promotion_mask = board.side == white ? 0xFF : 0xFF00000000000000;
        U64 push_promotions = push & promotion_mask & info.check_rays;
        U64 push_no_promotion = push & ~promotion_mask & info.check_rays;
        U64 target_row = board.side == white ? 0xFF00000000 : 0xFF000000;
        U64 double_push = shift(push, 8 * direction) & empty_squares & target_row & info.check_rays;

        U64 edge_mask_1 = board.side == white ? not_h_file : not_a_file;
        U64 edge_mask_2 = board.side == white ? not_a_file : not_h_file;
        U64 capture_mask_1 = shift((bitboard & edge_mask_1), 7 * direction) & board.occupancies[enemy];
        U64 capture_mask_2 = shift((bitboard & edge_mask_2), 9 * direction) & board.occupancies[enemy];
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
                    moves[move_index++] = encode_move(source, target, piece, no_promotion, no_piece, 0, 0, 0);
                }
                pop_bit(push_no_promotion, target);
            }
            while (double_push)
            {
                int target = least_significant_bit_index(double_push);
                int source = target + 16 * direction;
                if (!get_bit(info.pin_rays, source) || (align_mask[source][king_location] == align_mask[target][king_location]))
                {
                    moves[move_index++] = encode_move(source, target, piece, no_promotion, no_piece, 1, 0, 0);
                }
                pop_bit(double_push, target);
            }
        }

        // captures
        while (capture_no_promotion_1)
        {
            int target = least_significant_bit_index(capture_no_promotion_1);
            int source = target + 7 * direction;
            if (!get_bit(info.pin_rays, source) || (align_mask[source][king_location] == align_mask[target][king_location]))
            {
                moves[move_index++] = encode_move(source, target, piece, no_promotion, find_captured_piece(board, target), 0, 0, 0);
            }
            pop_bit(capture_no_promotion_1, target);
        }
        while (capture_no_promotion_2)
        {
            int target = least_significant_bit_index(capture_no_promotion_2);
            int source = target + 9 * direction;
            if (!get_bit(info.pin_rays, source) || (align_mask[source][king_location] == align_mask[target][king_location]))
            {
                moves[move_index++] = encode_move(source, target, piece, no_promotion, find_captured_piece(board, target), 0, 0, 0);
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
                moves[move_index++] = encode_move(source, target, piece, promotion_queen, no_piece, 0, 0, 0);
                moves[move_index++] = encode_move(source, target, piece, promotion_rook, no_piece, 0, 0, 0);
                moves[move_index++] = encode_move(source, target, piece, promotion_bishop, no_piece, 0, 0, 0);
                moves[move_index++] = encode_move(source, target, piece, promotion_knight, no_piece, 0, 0, 0);
            }
            pop_bit(push_promotions, target);
        }
        while (capture_promotions_1)
        {
            int target = least_significant_bit_index(capture_promotions_1);
            int source = target + 7 * direction;
            if (!get_bit(info.pin_rays, source) || (align_mask[source][king_location] == align_mask[target][king_location]))
            {
                moves[move_index++] = encode_move(source, target, piece, promotion_queen, find_captured_piece(board, target), 0, 0, 0);
                moves[move_index++] = encode_move(source, target, piece, promotion_rook, find_captured_piece(board, target), 0, 0, 0);
                moves[move_index++] = encode_move(source, target, piece, promotion_bishop, find_captured_piece(board, target), 0, 0, 0);
                moves[move_index++] = encode_move(source, target, piece, promotion_knight, find_captured_piece(board, target), 0, 0, 0);
            }
            pop_bit(capture_promotions_1, target);
        }
        while (capture_promotions_2)
        {
            int target = least_significant_bit_index(capture_promotions_2);
            int source = target + 9 * direction;
            if (!get_bit(info.pin_rays, source) || (align_mask[source][king_location] == align_mask[target][king_location]))
            {
                moves[move_index++] = encode_move(source, target, piece, promotion_queen, find_captured_piece(board, target), 0, 0, 0);
                moves[move_index++] = encode_move(source, target, piece, promotion_rook, find_captured_piece(board, target), 0, 0, 0);
                moves[move_index++] = encode_move(source, target, piece, promotion_bishop, find_captured_piece(board, target), 0, 0, 0);
                moves[move_index++] = encode_move(source, target, piece, promotion_knight, find_captured_piece(board, target), 0, 0, 0);
            }
            pop_bit(capture_promotions_2, target);
        }

        // en passant
        if (board.enpassant != no_square)
        {
            U64 capture_pawn_location = board.side == white ? board.enpassant + 8 : board.enpassant - 8;
            U64 capture_board = 1ULL << capture_pawn_location;
            if (capture_board & info.check_rays)
            {
                int enemy = board.side == white ? black : white;
                U64 pawns_able_to_en_passant = pawn_attacks(board.enpassant, enemy) & board.bitboards[piece];
                while (pawns_able_to_en_passant)
                {
                    int source = least_significant_bit_index(pawns_able_to_en_passant);
                    if (!get_bit(info.pin_rays, source) || (align_mask[source][king_location] == align_mask[board.enpassant][king_location]))
                    {
                        if (!in_check_after_en_passant(board, source, capture_pawn_location))
                        {
                            moves[move_index++] = encode_move(source, board.enpassant, piece, no_promotion, find_captured_piece(board, capture_pawn_location), 0, 1, 0);
                        }
                    }
                    pop_bit(pawns_able_to_en_passant, source);
                }
            }
        }
    }

    void _generate_king_moves(board_state &board, king_info &info, bool no_quiet_moves, span<unsigned int> moves, int &move_index)
    {
        int piece = (board.side == white) ? K : k;
        int rook = (board.side == white) ? R : r;
        int enemy_color = (board.side == white) ? black : white;

        // before using is_square_attacked, remove own king from the board
        U64 king_bitboard = board.bitboards[piece];
        int king_location = least_significant_bit_index(king_bitboard);
        board.bitboards[piece] = 0ULL;
        board.occupancies[board.side] ^= king_bitboard;
        board.occupancies[both] ^= king_bitboard;

        if (board.side == white && !no_quiet_moves)
        {
            // white kingside castle
            int castling_available = board.castle & wk;
            bool no_pieces_between = !get_bit(board.occupancies[both], f1) && !get_bit(board.occupancies[both], g1);
            bool king_not_through_check = !is_square_attacked(e1, board) && !is_square_attacked(f1, board) && !is_square_attacked(g1, board);
            bool king_and_rook_present = king_location == e1 && get_bit(board.bitboards[rook], h1);
            if (castling_available && no_pieces_between && king_not_through_check && king_and_rook_present)
            {
                moves[move_index++] = encode_move(e1, g1, piece, no_promotion, no_piece, 0, 0, 1);
            }

            // white queenside castle
            castling_available = board.castle & wq;
            no_pieces_between = !get_bit(board.occupancies[both], d1) && !get_bit(board.occupancies[both], c1) && !get_bit(board.occupancies[both], b1);
            king_not_through_check = !is_square_attacked(e1, board) && !is_square_attacked(d1, board) && !is_square_attacked(c1, board);
            king_and_rook_present = king_location == e1 && get_bit(board.bitboards[rook], a1);
            if (castling_available && no_pieces_between && king_not_through_check && king_and_rook_present)
            {
                moves[move_index++] = encode_move(e1, c1, piece, no_promotion, no_piece, 0, 0, 1);
            }
        }
        else if (board.side == black && !no_quiet_moves)
        {
            // black kingside castle
            int castling_available = board.castle & bk;
            bool no_pieces_between = !get_bit(board.occupancies[both], f8) && !get_bit(board.occupancies[both], g8);
            bool king_not_through_check = !is_square_attacked(e8, board) && !is_square_attacked(f8, board) && !is_square_attacked(g8, board);
            bool king_and_rook_present = king_location == e8 && get_bit(board.bitboards[rook], h8);
            if (castling_available && no_pieces_between && king_not_through_check && king_and_rook_present)
            {
                moves[move_index++] = encode_move(e8, g8, piece, no_promotion, no_piece, 0, 0, 1);
            }

            // black queenside castle
            castling_available = board.castle & bq;
            no_pieces_between = !get_bit(board.occupancies[both], d8) && !get_bit(board.occupancies[both], c8) && !get_bit(board.occupancies[both], b8);
            king_not_through_check = !is_square_attacked(e8, board) && !is_square_attacked(d8, board) && !is_square_attacked(c8, board);
            king_and_rook_present = king_location == e8 && get_bit(board.bitboards[rook], a8);
            if (castling_available && no_pieces_between && king_not_through_check && king_and_rook_present)
            {
                moves[move_index++] = encode_move(e8, c8, piece, no_promotion, no_piece, 0, 0, 1);
            }
        }

        // normal king moves
        U64 bitboard = king_bitboard;
        U64 attacks_board = king_attacks(king_location) & ~board.occupancies[board.side];
        if (no_quiet_moves)
            attacks_board &= board.occupancies[enemy_color];

        while (attacks_board)
        {
            int target = least_significant_bit_index(attacks_board);
            if (!is_square_attacked(target, board))
            {
                if (!get_bit(board.occupancies[enemy_color], target))
                    // non-capture
                    moves[move_index++] = encode_move(king_location, target, piece, no_promotion, no_piece, 0, 0, 0);
                else
                    // capture
                    moves[move_index++] = encode_move(king_location, target, piece, no_promotion, find_captured_piece(board, target), 0, 0, 0);
            }
            pop_bit(attacks_board, target);
        }

        // put the friendly king back on the board
        board.bitboards[piece] = king_bitboard;
        board.occupancies[board.side] ^= king_bitboard;
        board.occupancies[both] ^= king_bitboard;
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
                    moves[move_index++] = encode_move(source, target, piece, no_promotion, no_piece, 0, 0, 0);
                else
                    // capture
                    moves[move_index++] = encode_move(source, target, piece, no_promotion, find_captured_piece(board, target), 0, 0, 0);
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
                    moves[move_index++] = encode_move(source, target, piece, no_promotion, find_captured_piece(board, target), 0, 0, 0);
                else
                    // non-capture
                    moves[move_index++] = encode_move(source, target, piece, no_promotion, no_piece, 0, 0, 0);
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
                // if (get_bit(board.occupancies[enemy_color], target))
                //     cout << piece_to_string[find_captured_piece(board, target)] << endl;
                if (get_bit(board.occupancies[enemy_color], target))
                    // capture
                    moves[move_index++] = encode_move(source, target, piece, no_promotion, find_captured_piece(board, target), 0, 0, 0);
                else
                    // non-capture
                    moves[move_index++] = encode_move(source, target, piece, no_promotion, no_piece, 0, 0, 0);
                pop_bit(attacks_board, target);
            }
            pop_bit(diagonal_bitboard, source);
        }
    }

    king_info _find_check_and_pin_masks(board_state &board)
    {
        king_info info = king_info{};

        int king = (board.side == white) ? K : k;
        int king_location = least_significant_bit_index(board.bitboards[king]);
        U64 own_pieces = (board.side == white) ? board.occupancies[white] : board.occupancies[black];
        U64 enemy_pieces = (board.side == white) ? board.occupancies[black] : board.occupancies[white];
        int king_row = king_location / 8;
        int king_file = king_location % 8;
        int _r, _f;
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
            _r = king_row;
            _f = king_file;
            while (king_row + k * offsets[i][0] >= 0 && king_row + k * offsets[i][0] <= 7 && king_file + k * offsets[i][1] >= 0 && king_file + k * offsets[i][1] <= 7)
            {
                _r = king_row + k * offsets[i][0];
                _f = king_file + k * offsets[i][1];
                square = 8 * _r + _f;
                set_bit(ray_mask, square);
                if (get_bit(board.occupancies[both], square) == 0)  // no piece
                {
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
}
