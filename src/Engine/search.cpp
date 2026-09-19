#include "Engine/engine.h"
#include "Engine/evaluation.h"
#include "MoveGenerator/MoveGenerator.h"
#include <algorithm>
using namespace constants;
using namespace bitboard_utils;
using namespace transposition_table;

int Engine::negamax(board::board_state& position, int alpha, int beta, int depth,
                    int ply, int extensions, SearchPath path) {
    if (stopped()) return 0;
    const bool game_path = path == SearchPath::Game;
    if (!options.alpha_beta_pruning) { alpha = -alpha_beta_bounds_start; beta = alpha_beta_bounds_start; }
    pv_length[ply] = ply;
    if (depth <= 0) return quiescence(position, alpha, beta, ply, path);
    ++nodes;
    const bool in_check = move_generator::is_square_attacked(
        least_significant_bit_index(position.bitboards[position.side == white ? K : k]), position);
    std::array<unsigned int, max_moves> buffer;
    auto moves = move_generator::generate_moves(position, buffer, false);
    if (moves.empty()) return in_check ? -check_mate_score + ply : 0;
    if (game_path && drawn(position)) return 0;
    if (ply >= tuning::max_ply - 1) return evaluation::evaluate(position);

    const int original_alpha = alpha;
    const bool pv_node = beta - alpha > 1;
    const U64 key = table_key(position);
    const auto entry = options.transposition_table && game_path ? table.probe(key) : Entry{};
    if (entry.depth >= 0) ++statistics.transposition_table;
    if (options.alpha_beta_pruning && options.tt_cutoffs && ply > 0 && !pv_node && entry.depth >= depth) {
        int value = Table::score_at_ply(entry.score, ply);
        if (entry.bound == exact || (entry.bound == lowerbound && value >= beta) ||
            (entry.bound == upperbound && value <= alpha)) { ++statistics.tt_cutoffs; return value; }
    }
    // Null moves are synthetic positions: never use them for repetition or TT
    // scores, never allow consecutive nulls, and avoid pawn-only zugzwang cases.
    const int first = position.side == white ? N : n;
    U64 non_pawns = position.bitboards[first] | position.bitboards[first + 1] |
                    position.bitboards[first + 2] | position.bitboards[first + 3];
    if (options.alpha_beta_pruning && options.null_move_pruning && game_path && !pv_node &&
        !in_check && depth >= tuning::null_min_depth && non_pawns &&
        std::abs(beta) < check_mate_score - tuning::max_ply &&
        evaluation::evaluate(position) >= beta) {
        ++statistics.null_probes;
        auto next = board::make_null_move(position);
        int value = -negamax(next, -beta, -beta + 1, depth - 1 - tuning::null_reduction,
                             ply + 1, extensions, SearchPath::NullMoveProbe);
        if (aborted) return 0;
        if (value >= beta) { ++statistics.null_move_pruning; return beta; }
    }
    sort_moves(moves, entry.move, ply);
    int best = -alpha_beta_bounds_start;
    unsigned int best_move = invalid_move;
    int searched = 0;
    for (auto move : moves) {
        if (ply == 0 && limits.restrict_root_moves &&
            std::find(limits.root_moves.begin(), limits.root_moves.end(), move) == limits.root_moves.end()) continue;
        auto next = board::make_move(position, move);
        const bool gives_check = move_generator::is_square_attacked(
            least_significant_bit_index(next.bitboards[next.side == white ? K : k]), next);
        int extension = 0;
        const int piece = board::move_piece(move), target = board::move_target(move);
        if (extensions < tuning::max_extensions) {
            if (options.check_extensions && gives_check) { extension = 1; ++statistics.check_extensions; }
            else if (options.forced_move_extensions && moves.size() == 1) { extension = 1; ++statistics.forced_move_extensions; }
            else if (options.pawn_extensions && ((piece == P && target / 8 == 1) || (piece == p && target / 8 == 6))) {
                extension = 1; ++statistics.pawn_extensions;
            }
        }
        const bool quiet = board::move_capture(move) == no_piece && board::move_promotion(move) == no_promotion;
        int child_depth = depth - 1 + extension;
        int reduction = options.alpha_beta_pruning && options.late_move_reductions && searched >= tuning::lmr_first_move &&
            depth >= tuning::lmr_min_depth && quiet && !in_check && !gives_check && !extension
            ? std::min(child_depth - 1, 1 + (searched > tuning::lmr_second_threshold) + (searched > tuning::lmr_third_threshold)) : 0;
        history.push_back(board_utils::repetition_key(next));
        int value;
        if (reduction > 0) {
            ++statistics.late_move_reductions;
            value = -negamax(next, -alpha - 1, -alpha, child_depth - reduction,
                             ply + 1, extensions + extension, path);
        } else value = alpha + 1;
        if (value > alpha) {
            if (options.alpha_beta_pruning && options.principal_variation_search && searched > 0) {
                ++statistics.principal_variation_search;
                value = -negamax(next, -alpha - 1, -alpha, child_depth,
                                 ply + 1, extensions + extension, path);
                if (!aborted && value > alpha && value < beta)
                    value = -negamax(next, -beta, -alpha, child_depth,
                                     ply + 1, extensions + extension, path);
            } else {
                value = -negamax(next, -beta, -alpha, child_depth,
                                 ply + 1, extensions + extension, path);
            }
        }
        history.pop_back();
        if (aborted) return 0;
        ++searched;
        if (value > best) { best = value; best_move = move; }
        if (value > alpha) { alpha = value; update_pv(move, ply); }
        if (options.alpha_beta_pruning && alpha >= beta) {
            ++statistics.alpha_beta_pruning;
            if (quiet) {
                if (options.killer_ordering && killers[ply][0] != move) { killers[ply][1] = killers[ply][0]; killers[ply][0] = move; }
                if (options.history_ordering) {
                    auto& bonus = history_scores[piece][target];
                    bonus = std::min(tuning::max_history, bonus + depth * depth);
                }
            }
            break;
        }
    }
    if (!searched) return 0;
    if (options.transposition_table && game_path && !(ply == 0 && limits.restrict_root_moves)) {
        Bound bound = best >= beta ? lowerbound : (best > original_alpha ? exact : upperbound);
        table.store(key, best_move, depth, bound, best, ply);
    }
    return best;
}
