#include "Engine/engine.h"
#include "Engine/evaluation.h"
#include "MoveGenerator/MoveGenerator.h"
#include <algorithm>
#include <iostream>
using namespace constants;

Engine::Engine(EngineSettings settings) : options(settings), table(settings.hash_mb) {}
void Engine::configure(EngineSettings settings) {
    if (settings.hash_mb != options.hash_mb) table.resize(settings.hash_mb);
    else if (std::any_of(boolean_options.begin(), boolean_options.end(),
                        [&](const auto& option) { return settings.*option.member != options.*option.member; }))
        table.clear(); // search semantics changed; timing-only changes preserve it
    options = settings;
}
bool Engine::stopped() {
    if (aborted) return true;
    if (event_pump && (++poll_ticks & 1023U) == 0) event_pump();
    if ((stop_flag && stop_flag->load(std::memory_order_relaxed)) ||
        (limits.nodes && nodes >= limits.nodes) ||
        (limits.movetime_ms > 0 && std::chrono::steady_clock::now() - start >=
         std::chrono::milliseconds(limits.movetime_ms))) aborted = true;
    return aborted;
}
bool Engine::drawn(const board::board_state& board) const {
    if (board.halfmove_clock >= 100 || evaluation::insufficient_material(board)) return true;
    int count = 0;
    const size_t begin = history.size() > static_cast<size_t>(board.halfmove_clock + 1)
        ? history.size() - board.halfmove_clock - 1 : 0;
    for (size_t i = begin; i < history.size(); ++i)
        if (history[i] == history.back() && ++count >= 3) return true;
    return false;
}
U64 Engine::table_key(const board::board_state& board) const {
    // Repetition and the fifty-move rule are path dependent. Include reversible
    // history and the clock so cached draw scores cannot leak between histories.
    U64 key = 0xcbf29ce484222325ULL ^ (static_cast<U64>(board.halfmove_clock) * 0x9e3779b97f4a7c15ULL);
    const size_t begin = history.size() > static_cast<size_t>(board.halfmove_clock + 1)
        ? history.size() - board.halfmove_clock - 1 : 0;
    for (size_t i = begin; i < history.size(); ++i)
        key = (key ^ history[i]) * 0x100000001b3ULL;
    return key ^ board.zobrist_hash;
}
void Engine::update_pv(unsigned int move, int ply) {
    pv[ply][ply] = move;
    pv_length[ply] = ply + 1;
    if (ply + 1 < tuning::max_ply) {
        for (int i = ply + 1; i < pv_length[ply + 1]; ++i) pv[ply][i] = pv[ply + 1][i];
        pv_length[ply] = std::max(pv_length[ply], pv_length[ply + 1]);
    }
}
SearchResult Engine::search(board::board_state board, const SearchLimits& requested,
                           const std::atomic_bool* stop,
                           std::function<void(const SearchResult&)> on_iteration) {
    limits = requested;
    limits.depth = std::clamp(limits.depth, 1, tuning::max_ply - 1);
    stop_flag = stop;
    aborted = false;
    nodes = 0;
    start = std::chrono::steady_clock::now();
    result = {};
    statistics = {};
    pv = {}; pv_length = {}; killers = {}; history_scores = {};
    // Retain transpositions between moves. Independent experiments explicitly
    // call clear_hash()/ucinewgame; changing search options also invalidates it.
    const auto saved_history = history;
    const U64 root_key = board_utils::repetition_key(board);
    if (history.empty() || history.back() != root_key) history.push_back(root_key);
    std::array<unsigned int, max_moves> buffer;
    auto moves = move_generator::generate_moves(board, buffer, false);
    for (auto move : moves) {
        if (!limits.restrict_root_moves || std::find(limits.root_moves.begin(), limits.root_moves.end(), move) != limits.root_moves.end()) {
            result.best_move = move; result.pv = {move}; break;
        }
    }
    if (result.best_move != invalid_move || moves.empty()) {
        for (int depth = options.iterative_deepening ? 1 : limits.depth; depth <= limits.depth && !stopped(); ++depth) {
            int margin = options.alpha_beta_pruning && options.aspiration_windows && options.iterative_deepening && depth > 1 ? tuning::aspiration_margin : alpha_beta_bounds_start;
            int score = 0;
            while (!stopped()) {
                int alpha = std::max(-alpha_beta_bounds_start, result.score - margin);
                int beta = std::min(alpha_beta_bounds_start, result.score + margin);
                score = negamax(board, alpha, beta, depth, 0, 0, SearchPath::Game);
                if (aborted) break;
                if (score <= alpha || score >= beta) { ++statistics.aspiration_windows; margin = std::min(alpha_beta_bounds_start * 2, margin * 2); continue; }
                break;
            }
            if (aborted) break; // publish only a fully completed iteration
            if (options.iterative_deepening) ++statistics.iterative_deepening;
            result.statistics = statistics;
            result.score = score; result.depth = depth;
            if (pv_length[0] > 0) {
                result.pv.assign(pv[0].begin(), pv[0].begin() + pv_length[0]);
                result.best_move = result.pv.front();
            }
            result.nodes = nodes;
            result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            if (on_iteration) on_iteration(result);
            if (moves.empty() || drawn(board)) break;
        }
    }
    result.nodes = nodes;
    result.statistics = statistics;
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    history = saved_history;
    stop_flag = nullptr;
    return result;
}
int Engine::iterative_search(board::board_state& board, int milliseconds) {
    SearchLimits requested;
    requested.movetime_ms = std::max(1, milliseconds);
    return search(board, requested).score;
}
void Engine::print_principal_variation() const {
    for (auto move : result.pv) std::cout << board::move_to_string(move) << ' ';
    std::cout << '\n';
}
