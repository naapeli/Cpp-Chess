#include "Engine/engine.h"
#include "Engine/transpositionTable.h"
#include "MoveGenerator/MoveGenerator.h"
#include "Board/board.h"
#include "utils.h"

#include <iostream>
#include <array>
#include <vector>
#include <span>
#include <chrono>

using move_generator::generate_moves, move_generator::is_square_attacked, move_generator::print_move_list;
using board::make_move, board::move_to_string, board::move_capture;
using board::move_piece, board::move_promotion, board::move_target;
using board::is_promoting;
using namespace constants;
using namespace bitboard_utils;
using transposition_table::add_move_to_table, transposition_table::get_evaluation_from_table;
using transposition_table::exact, transposition_table::lowerbound, transposition_table::upperbound;

using std::cout, std::endl;
using std::array, std::vector, std::span;


int Engine::nodes_searched() { return nodes; }
unsigned int Engine::best_move() { return pv_table[0][0]; }
bool Engine::time_up() { return (std::chrono::steady_clock::now() - search_start_time) > time_limit; }
int Engine::iterative_search(board_state &board, int time_milli_seconds)
{
    time_limit = std::chrono::milliseconds{time_milli_seconds};
    int depth = 10000;
    search_start_time = std::chrono::steady_clock::now();
    bool in_check = is_square_attacked(board.side == white ? least_significant_bit_index(board.bitboards[K]) : least_significant_bit_index(board.bitboards[k]), board);

    int evaluation;
    int middle = 0;
    int lower_window = alpha_beta_bounds_start;
    int upper_window = alpha_beta_bounds_start;
    int alpha;
    int beta;
    for (int iterative_depth = 1; iterative_depth <= depth; iterative_depth++)
    {
        nodes = 0;
        while (true)
        {
            alpha = middle - lower_window;
            beta = middle + upper_window;
            evaluation = negamax(board, alpha, beta, iterative_depth, 0, 0, in_check, false);
            if (time_up()) break;

            if (evaluation >= beta)
            {
                cout << "Aspiration window failed high!" << endl;
                upper_window *= 3;
                continue;
            }
            if (evaluation <= alpha)
            {
                cout << "Aspiration window failed low!" << endl;
                lower_window *= 3;
                continue;
            }

            cout << "Depth: " << iterative_depth;
            cout << " Evaluation: " << evaluation << " cp";
            cout << " Nodes: " << nodes_searched();
            cout << " Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - search_start_time);
            cout << " Principal variation: ";
            print_principal_variation();

            lower_window = material_score[P] / 2;
            upper_window = material_score[P] / 2;
            middle = evaluation;
            break;
        }

        if (time_up()) break;

    }
    return evaluation;
}
void Engine::print_principal_variation()
{
    for (int i = 0; i < pv_length[0]; i++)
    {
        cout << move_to_string(pv_table[0][i]) << " ";
    }
    cout << endl;
}

int Engine::negamax(board_state &board, int alpha, int beta, int depth, int depth_from_root, int total_extension, bool in_check, bool allow_pruning)
{
    if (time_up()) return invalid_evaluation;
    nodes++;
    pv_length[depth_from_root] = depth_from_root;

    int table_evaluation = get_evaluation_from_table(board.zobrist_hash, depth, alpha, beta);
    if (table_evaluation != invalid_evaluation)
    {
        return table_evaluation;
    }

    if (depth <= 0)
    {
        int evaluation = quiescence_search(board, alpha, beta);
        return evaluation;
    }

    int static_eval = evaluate(board);

    // null move pruning
    bool not_pv = alpha == beta - 1;
    if (allow_pruning && !in_check && depth >= 3 && not_pv)
    {
        // count the remaining number of enemy pieces
        int pieces_remaining = 0;
        int start = board.side == white ? 7 : 1;
        int end = board.side == white ? 10 : 4;
        for (int piece = start; piece <= end; piece++)
        {
            U64 bitboard = board.bitboards[piece];
            pieces_remaining += count_bits(bitboard);
            if (pieces_remaining > 2) break;
        }
        if (pieces_remaining > 2)
        {
            int en_passant_square = board.enpassant;
            board.enpassant = no_square;
            board.side ^= 1;

            int evaluation = -negamax(board, -beta, -beta + 1, depth - 3, depth_from_root + 1, total_extension, false, false);

            board.enpassant = en_passant_square;
            board.side ^= 1;

            if (time_up()) return invalid_evaluation;
            if (evaluation >= beta) return beta;
        }
    }

    bool found_pv_node = false;
    int node_type = upperbound;
    array<unsigned int, max_moves> move_list;
    span<unsigned int> moves = generate_moves(board, move_list, false);
    sort_moves(moves, !not_pv, depth_from_root);
    for (int i = 0; i < moves.size(); i++)
    {
        unsigned int move = moves[i];
        board_state next_state = make_move(board, move);

        bool move_in_check = is_square_attacked(next_state.side == white ? least_significant_bit_index(next_state.bitboards[K]) : least_significant_bit_index(next_state.bitboards[k]), next_state);

        int extension = search_extension(move, total_extension, move_in_check, moves.size());
        int move_total_extension = total_extension + extension;

        // principal variation search
        int evaluation;
        int used_depth = depth;
        if (found_pv_node)
        {
            int reduction = late_move_reduction(move, depth, i, extension);
            if (reduction > 0)
                evaluation = -negamax(next_state, -alpha - 1, -alpha, depth - 1 + extension - reduction, depth_from_root + 1, move_total_extension, move_in_check, true);
                if (time_up()) return invalid_evaluation;
            else
                evaluation = alpha + 1;

            if (evaluation > alpha)
            {
                evaluation = -negamax(next_state, -alpha - 1, -alpha, depth - 1 + extension, depth_from_root + 1, move_total_extension, move_in_check, true);
                if (time_up()) return invalid_evaluation;
                if (evaluation > alpha && evaluation < beta)
                    evaluation = -negamax(next_state, -beta, -alpha, depth - 1 + extension, depth_from_root + 1, move_total_extension, move_in_check, true);
                    if (time_up()) return invalid_evaluation;
            }
        }
        else
            evaluation = -negamax(next_state, -beta, -alpha, depth - 1 + extension, depth_from_root + 1, move_total_extension, move_in_check, true);
            if (time_up()) return invalid_evaluation;

        if (evaluation >= beta)
        {
            add_move_to_table(board.zobrist_hash, move, depth, lowerbound, evaluation);

            // store killer moves
            killer_moves[1][depth_from_root] = killer_moves[0][depth_from_root];
            killer_moves[0][depth_from_root] = move;

            return beta;
        }
        if (evaluation > alpha)
        {
            found_pv_node = true;
            node_type = exact;

            // store history heuristic
            if (move_capture(move) == no_piece)
            {
                history_moves[move_piece(move)][move_target(move)] += depth;
            }

            // update the principal variation
            pv_table[depth_from_root][depth_from_root] = move;
            for (int next_depth = depth_from_root + 1; next_depth < pv_length[depth_from_root + 1]; next_depth++)
            {
                pv_table[depth_from_root][next_depth] = pv_table[depth_from_root + 1][next_depth];
            }
            pv_length[depth_from_root] = pv_length[depth_from_root + 1];

            // update lower bound for the best move
            alpha = evaluation;
        }
    }
    if (moves.size() == 0)
    {
        if (in_check)
            return -check_mate_score + depth_from_root;
        return 0;  // stalemate
    }

    unsigned int best_move = pv_table[depth_from_root][depth_from_root];
    add_move_to_table(board.zobrist_hash, best_move, depth, node_type, alpha);

    return alpha;
}

int Engine::quiescence_search(board_state &board, int alpha, int beta)
{
    if (time_up()) return invalid_evaluation;
    nodes++;
    int evaluation = evaluate(board);
    if(evaluation >= beta)
        return beta;

    int margin = delta_cutoff;  // margin = queen
    if ( is_promoting(board) ) margin += material_score[4] - material_score[0];  // margin = 2 * queen - pawn
    if ( evaluation < alpha - margin ) return alpha;
    
    if(alpha < evaluation)
        alpha = evaluation;

    array<unsigned int, max_moves> move_list;
    span<unsigned int> moves = generate_moves(board, move_list, true);
    sort_moves(moves, false, -1);
    for (int i = 0; i < moves.size(); i++)
    {
        unsigned int move = moves[i];
        board_state next_state = make_move(board, move);
        int evaluation = -quiescence_search(next_state, -beta, -alpha);
        if (time_up()) return invalid_evaluation;

        if (evaluation >= beta)
        {
            return beta;
        }
        if (evaluation > alpha)
        {
            alpha = evaluation;
        }
    }
    return alpha;
}

int Engine::evaluate(board_state &board)
{
    int evaluation = 0;
    for (int piece = P; piece <= k; piece++)
    {
        U64 bitboard = board.bitboards[piece];
        while (bitboard)
        {
            int square = least_significant_bit_index(bitboard);
            evaluation += material_score[piece];

            if (piece <= K)
            {
                evaluation += mg_table[piece][square];
            }
            else
            {
                evaluation -= mg_table[piece - 6][mirror_score[square]];
            }

            pop_bit(bitboard, square);
        }
    }
    return board.side == white ? evaluation : -evaluation;
}

void Engine::sort_moves(span<unsigned int> moves, bool score_pv_move, int depth_from_root)
{
    vector<int> scores;
    scores.reserve(moves.size());  // reserve the correct amount of memory in advance

    for (int i = 0; i < moves.size(); i++)
    {
        // score the principal variation move
        unsigned int move = moves[i];
        if (score_pv_move && move == pv_table[0][depth_from_root])
        {
            scores.push_back(best_move_bonus);
            continue;
        }

        // score non quiet moves
        int promoted_piece = move_promotion(move);
        int promotion_bonus = promoted_piece == promotion_queen ? 800 : 
                                promoted_piece == promotion_rook ? 400 :
                                promoted_piece == promotion_bishop ? 250 :
                                promoted_piece == promotion_knight ? 200 : 0;
        int captured_piece = move_capture(move);
        if (captured_piece != no_piece)
        {
            scores.push_back(non_quiet_bonus + mvv_lva[move_piece(move)][captured_piece] + promotion_bonus);
            continue;
        }
        if (promoted_piece != no_promotion)
        {
            scores.push_back(non_quiet_bonus + promotion_bonus);
            continue;
        }

        // score quiet moves based on history and killer moves
        if (move == killer_moves[0][depth_from_root])
        {
            scores.push_back(first_killer_bonus);
            continue;
        }
        if (move == killer_moves[1][depth_from_root])
        {
            scores.push_back(second_killer_bonus);
            continue;
        }
        scores.push_back(history_moves[move_piece(move)][move_target(move)]);
    }

    for (size_t i = 1; i < moves.size(); ++i) {
        int score = scores[i];
        unsigned int move = moves[i];
        size_t j = i;
        while (j > 0 && scores[j - 1] < score) {
            scores[j] = scores[j - 1];
            moves[j] = moves[j - 1];
            j--;
        }
        scores[j] = score;
        moves[j] = move;
    }
}

int Engine::late_move_reduction(unsigned int move, int depth, int move_priority_index, int search_extension)
{
    if (move_priority_index < 4 || depth < 3 || search_extension > 0 || move_capture(move) != no_piece)
    {
        return 0;
    }

    if (move_priority_index > 20) return 3;
    if (move_priority_index > 10) return 2;
    return 1;
}

int Engine::search_extension(unsigned int move, int total_extension, bool in_check, int n_moves)
{
    if (total_extension > 16) return 0;
    if (in_check) return 1;
    int piece = move_piece(move);
    int target = move_target(move);
    if ((piece == P && (a7 <= target && target <= h7)) || (piece == p && (a2 <= target && target <= h2))) return 1;
    if (n_moves == 1) return 1;
    return 0;
}
