#include "Engine/engine.h"
#include "Engine/evaluation.h"
#include "MoveGenerator/MoveGenerator.h"
#include <algorithm>
using namespace constants;
using namespace bitboard_utils;

int Engine::quiescence(board::board_state& position, int alpha, int beta, int ply, SearchPath path) {
    if (stopped()) return 0;
    if (!options.alpha_beta_pruning) { alpha = -alpha_beta_bounds_start; beta = alpha_beta_bounds_start; }
    ++nodes;
    pv_length[ply] = ply;
    const bool in_check = move_generator::is_square_attacked(
        least_significant_bit_index(position.bitboards[position.side == white ? K : k]), position);
    // Generate all legal moves before stand pat, so horizon stalemate and mate
    // are recognized, and every evasion is available while in check.
    std::array<unsigned int, max_moves> buffer;
    auto moves = move_generator::generate_moves(position, buffer, false);
    if (moves.empty()) return in_check ? -check_mate_score + ply : 0;
    if (path == SearchPath::Game && drawn(position)) return 0;
    int stand_pat = evaluation::evaluate(position);
    if (ply >= tuning::max_ply - 1) return stand_pat;
    if (!in_check) {
        if (!options.quiescence) return stand_pat;
        if (options.alpha_beta_pruning && stand_pat >= beta) {
            if (options.stand_pat_pruning) { ++statistics.stand_pat_pruning; return stand_pat; }
            // Keep evaluating captures when this shortcut is disabled. Widen
            // beta so stand pat cannot create an inverted child window.
            beta = alpha_beta_bounds_start;
        }
        alpha = std::max(alpha, stand_pat);
    }
    if (options.quiescence) ++statistics.quiescence;
    sort_moves(moves, invalid_move, ply);
    for (auto move : moves) {
        const bool promotion = board::move_promotion(move) != no_promotion;
        if (!in_check && board::move_capture(move) == no_piece && !promotion) continue;
        auto next = board::make_move(position, move);
        const bool gives_check = move_generator::is_square_attacked(
            least_significant_bit_index(next.bitboards[next.side == white ? K : k]), next);
        if (options.alpha_beta_pruning && options.quiescence && options.delta_pruning && !in_check && !promotion && !gives_check &&
            std::abs(alpha) < check_mate_score - tuning::max_ply &&
            stand_pat + tuning::delta_margin < alpha) { ++statistics.delta_pruning; continue; }
        history.push_back(board_utils::repetition_key(next));
        int value = -quiescence(next, -beta, -alpha, ply + 1, path);
        history.pop_back();
        if (aborted) return 0;
        if (options.alpha_beta_pruning && value >= beta) { ++statistics.alpha_beta_pruning; return value; }
        if (value > alpha) { alpha = value; update_pv(move, ply); }
    }
    return alpha;
}
