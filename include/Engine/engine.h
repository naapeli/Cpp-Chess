#pragma once
#include "Board/board.h"
#include "Engine/settings.h"
#include "Engine/transpositionTable.h"
#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <span>
#include <vector>

struct SearchLimits {
    int depth = tuning::max_ply - 1;
    int movetime_ms = 0; // zero means no clock limit
    U64 nodes = 0;      // zero means no node limit
    std::vector<unsigned int> root_moves;
    bool restrict_root_moves = false;
};
struct SearchResult {
    unsigned int best_move = constants::invalid_move;
    int score = 0;
    int depth = 0;
    U64 nodes = 0;
    long long elapsed_ms = 0;
    std::vector<unsigned int> pv;
    SearchStatistics statistics;
};

class Engine {
public:
    explicit Engine(EngineSettings settings = {});
    // on_iteration receives the last fully completed depth, for UCI progress.
    // It is synchronous; an interrupted depth never replaces that result.
    SearchResult search(board::board_state board, const SearchLimits& limits,
                        const std::atomic_bool* stop = nullptr,
                        std::function<void(const SearchResult&)> on_iteration = {});
    int iterative_search(board::board_state& board, int time_milli_seconds);
    U64 nodes_searched() const { return result.nodes; }
    unsigned int best_move() const { return result.best_move; }
    void print_principal_variation() const;
    void increment_repetition(U64 key) { history.push_back(key); }
    void clear_repetition_table() { history.clear(); }
    void set_history(std::vector<U64> keys) { history = std::move(keys); }
    void clear_hash() { table.clear(); }
    void configure(EngineSettings settings);
    // Host scheduling hook, used by the browser worker to receive UCI stop.
    void set_event_pump(std::function<void()> pump) { event_pump = std::move(pump); }
    const EngineSettings& settings() const { return options; }
private:
    // A null-move probe contains a hypothetical pass, not legal game history.
    // Descendants (including quiescence) cannot claim history-based draws or
    // read/write game-path TT scores. This is search context, not a heuristic.
    enum class SearchPath { Game, NullMoveProbe };
    int negamax(board::board_state& board, int alpha, int beta, int depth,
                int ply, int extensions, SearchPath path);
    int quiescence(board::board_state& board, int alpha, int beta, int ply, SearchPath path);
    void sort_moves(std::span<unsigned int> moves, unsigned int tt_move, int ply);
    bool stopped();
    bool drawn(const board::board_state& board) const;
    U64 table_key(const board::board_state& board) const;
    void update_pv(unsigned int move, int ply);
    EngineSettings options;
    transposition_table::Table table;
    SearchLimits limits;
    SearchResult result;
    const std::atomic_bool* stop_flag = nullptr;
    bool aborted = false;
    std::function<void()> event_pump;
    unsigned int poll_ticks = 0;
    U64 nodes = 0;
    SearchStatistics statistics;
    std::chrono::steady_clock::time_point start;
    std::vector<U64> history; // chronological repetition keys, including current position
    std::array<std::array<unsigned int, tuning::max_ply>, tuning::max_ply> pv{};
    std::array<int, tuning::max_ply> pv_length{};
    std::array<std::array<unsigned int, 2>, tuning::max_ply> killers{};
    std::array<std::array<int, 64>, 12> history_scores{};
};
