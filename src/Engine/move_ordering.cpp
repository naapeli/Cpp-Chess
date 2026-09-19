#include "Engine/engine.h"
#include <algorithm>
using namespace constants;

void Engine::sort_moves(std::span<unsigned int> moves, unsigned int tt_move, int ply) {
    std::array<int, max_moves> scores{};
    for (size_t i = 0; i < moves.size(); ++i) {
        auto move = moves[i];
        int capture = board::move_capture(move), promotion = board::move_promotion(move);
        if (options.tt_move_ordering && move == tt_move && move != invalid_move) {
            scores[i] = tuning::tt_move_bonus; ++statistics.tt_move_ordering;
        } else if ((options.capture_ordering && capture != no_piece) || (options.promotion_ordering && promotion != no_promotion)) {
            scores[i] = tuning::tactical_bonus;
            if (options.capture_ordering && capture != no_piece) {
                scores[i] += tuning::mvv_lva_victim_scale * (capture % 6 + 1) - board::move_piece(move) % 6;
                ++statistics.capture_ordering;
            }
            if (options.promotion_ordering && promotion != no_promotion) {
                int type = promotion == promotion_queen ? Q : promotion == promotion_rook ? R : promotion == promotion_bishop ? B : N;
                scores[i] += tuning::mg_material[type]; ++statistics.promotion_ordering;
            }
        } else if (capture == no_piece && promotion == no_promotion) {
            if (options.killer_ordering && move == killers[ply][0]) { scores[i] = tuning::first_killer_bonus; ++statistics.killer_ordering; }
            else if (options.killer_ordering && move == killers[ply][1]) { scores[i] = tuning::second_killer_bonus; ++statistics.killer_ordering; }
            else if (options.history_ordering) {
                scores[i] = history_scores[board::move_piece(move)][board::move_target(move)];
                if (scores[i]) ++statistics.history_ordering;
            }
        }
    }
    for (size_t i = 1; i < moves.size(); ++i) {
        auto move = moves[i]; int score = scores[i]; size_t j = i;
        while (j > 0 && scores[j - 1] < score) { moves[j] = moves[j - 1]; scores[j] = scores[j - 1]; --j; }
        moves[j] = move; scores[j] = score;
    }
}
