#pragma once

#include <array>
#include <cstdint>
#include <string_view>

// All engine tuning defaults live here. UCI setoption overrides the runtime
// fields for one process; changing this file changes native and browser defaults.
struct EngineSettings {
    bool alpha_beta_pruning = true;
    bool null_move_pruning = true;
    bool late_move_reductions = true;
    bool delta_pruning = true;
    bool stand_pat_pruning = true;
    bool aspiration_windows = true;
    bool principal_variation_search = true;
    bool transposition_table = true;
    bool tt_cutoffs = true;
    bool tt_move_ordering = true;
    bool quiescence = true;
    bool check_extensions = true;
    bool pawn_extensions = true;
    bool forced_move_extensions = true;
    bool capture_ordering = true;
    bool promotion_ordering = true;
    bool killer_ordering = true;
    bool history_ordering = true;
    bool iterative_deepening = true;
    int hash_mb = 16;
    int move_overhead_ms = 20;
};

// Cumulative events, including work in abandoned/researched iterations.
struct SearchStatistics {
    std::uint64_t alpha_beta_pruning = 0;
    std::uint64_t null_move_pruning = 0;
    std::uint64_t late_move_reductions = 0;
    std::uint64_t delta_pruning = 0;
    std::uint64_t stand_pat_pruning = 0;
    std::uint64_t aspiration_windows = 0;
    std::uint64_t principal_variation_search = 0;
    std::uint64_t transposition_table = 0;
    std::uint64_t tt_cutoffs = 0;
    std::uint64_t tt_move_ordering = 0;
    std::uint64_t quiescence = 0;
    std::uint64_t check_extensions = 0;
    std::uint64_t pawn_extensions = 0;
    std::uint64_t forced_move_extensions = 0;
    std::uint64_t capture_ordering = 0;
    std::uint64_t promotion_ordering = 0;
    std::uint64_t killer_ordering = 0;
    std::uint64_t history_ordering = 0;
    std::uint64_t iterative_deepening = 0;
    std::uint64_t null_probes = 0;
};

struct BooleanOption {
    std::string_view name;
    bool EngineSettings::*member;
    std::uint64_t SearchStatistics::*events;
};
inline constexpr std::array boolean_options = {
    BooleanOption{"AlphaBetaPruning", &EngineSettings::alpha_beta_pruning, &SearchStatistics::alpha_beta_pruning},
    BooleanOption{"NullMovePruning", &EngineSettings::null_move_pruning, &SearchStatistics::null_move_pruning},
    BooleanOption{"LateMoveReductions", &EngineSettings::late_move_reductions, &SearchStatistics::late_move_reductions},
    BooleanOption{"DeltaPruning", &EngineSettings::delta_pruning, &SearchStatistics::delta_pruning},
    BooleanOption{"StandPatPruning", &EngineSettings::stand_pat_pruning, &SearchStatistics::stand_pat_pruning},
    BooleanOption{"AspirationWindows", &EngineSettings::aspiration_windows, &SearchStatistics::aspiration_windows},
    BooleanOption{"PrincipalVariationSearch", &EngineSettings::principal_variation_search, &SearchStatistics::principal_variation_search},
    BooleanOption{"TranspositionTable", &EngineSettings::transposition_table, &SearchStatistics::transposition_table},
    BooleanOption{"TTCutoffs", &EngineSettings::tt_cutoffs, &SearchStatistics::tt_cutoffs},
    BooleanOption{"TTMoveOrdering", &EngineSettings::tt_move_ordering, &SearchStatistics::tt_move_ordering},
    BooleanOption{"Quiescence", &EngineSettings::quiescence, &SearchStatistics::quiescence},
    BooleanOption{"CheckExtensions", &EngineSettings::check_extensions, &SearchStatistics::check_extensions},
    BooleanOption{"PawnExtensions", &EngineSettings::pawn_extensions, &SearchStatistics::pawn_extensions},
    BooleanOption{"ForcedMoveExtensions", &EngineSettings::forced_move_extensions, &SearchStatistics::forced_move_extensions},
    BooleanOption{"CaptureOrdering", &EngineSettings::capture_ordering, &SearchStatistics::capture_ordering},
    BooleanOption{"PromotionOrdering", &EngineSettings::promotion_ordering, &SearchStatistics::promotion_ordering},
    BooleanOption{"KillerOrdering", &EngineSettings::killer_ordering, &SearchStatistics::killer_ordering},
    BooleanOption{"HistoryOrdering", &EngineSettings::history_ordering, &SearchStatistics::history_ordering},
    BooleanOption{"IterativeDeepening", &EngineSettings::iterative_deepening, &SearchStatistics::iterative_deepening},
};

namespace tuning {
inline constexpr int max_ply = 128;
inline constexpr int max_extensions = 8;
inline constexpr int aspiration_margin = 40;
inline constexpr int null_min_depth = 3;
inline constexpr int null_reduction = 2;
inline constexpr int lmr_min_depth = 3;
inline constexpr int lmr_first_move = 4;
inline constexpr int lmr_second_threshold = 10;
inline constexpr int lmr_third_threshold = 20;
inline constexpr int mvv_lva_victim_scale = 10;
inline constexpr int increment_divisor = 2;
inline constexpr int delta_margin = 1100;
inline constexpr int default_moves_to_go = 30;
inline constexpr int max_hash_mb = 1024;
inline constexpr int max_history = 8000;
inline constexpr int tt_move_bonus = 100000;
inline constexpr int tactical_bonus = 10000;
inline constexpr int first_killer_bonus = 9000;
inline constexpr int second_killer_bonus = 8000;
inline constexpr std::array<int, 6> mg_material = {82, 337, 365, 477, 1025, 0};
inline constexpr std::array<int, 6> eg_material = {94, 281, 297, 512, 936, 0};
inline constexpr std::array<int, 6> phase_weights = {0, 1, 1, 2, 4, 0};
inline constexpr int total_phase = 24;
}

// Piece-square evaluation tuning (a8 through h1, white perspective).
namespace tuning {
        inline constexpr std::array<std::array<int, 64>, 6> mg_table = {{
            {
                0,   0,   0,   0,   0,   0,  0,   0,
                98, 134,  61,  95,  68, 126, 34, -11,
                -6,   7,  26,  31,  65,  56, 25, -20,
                -14,  13,   6,  21,  23,  12, 17, -23,
                -27,  -2,  -5,  12,  17,   6, 10, -25,
                -26,  -4,  -4, -10,   3,   3, 33, -12,
                -35,  -1, -20, -23, -15,  24, 38, -22,
                0,   0,   0,   0,   0,   0,  0,   0,
            },
            {
                -167, -89, -34, -49,  61, -97, -15, -107,
                -73, -41,  72,  36,  23,  62,   7,  -17,
                -47,  60,  37,  65,  84, 129,  73,   44,
                -9,  17,  19,  53,  37,  69,  18,   22,
                -13,   4,  16,  13,  28,  19,  21,   -8,
                -23,  -9,  12,  10,  19,  17,  25,  -16,
                -29, -53, -12,  -3,  -1,  18, -14,  -19,
                -105, -21, -58, -33, -17, -28, -19,  -23,
            },
            {
                -29,   4, -82, -37, -25, -42,   7,  -8,
                -26,  16, -18, -13,  30,  59,  18, -47,
                -16,  37,  43,  40,  35,  50,  37,  -2,
                -4,   5,  19,  50,  37,  37,   7,  -2,
                -6,  13,  13,  26,  34,  12,  10,   4,
                0,  15,  15,  15,  14,  27,  18,  10,
                4,  15,  16,   0,   7,  21,  33,   1,
                -33,  -3, -14, -21, -13, -12, -39, -21,
            },
            {
                32,  42,  32,  51, 63,  9,  31,  43,
                27,  32,  58,  62, 80, 67,  26,  44,
                -5,  19,  26,  36, 17, 45,  61,  16,
                -24, -11,   7,  26, 24, 35,  -8, -20,
                -36, -26, -12,  -1,  9, -7,   6, -23,
                -45, -25, -16, -17,  3,  0,  -5, -33,
                -44, -16, -20,  -9, -1, 11,  -6, -71,
                -19, -13,   1,  17, 16,  7, -37, -26,
            },
            {
                -28,   0,  29,  12,  59,  44,  43,  45,
                -24, -39,  -5,   1, -16,  57,  28,  54,
                -13, -17,   7,   8,  29,  56,  47,  57,
                -27, -27, -16, -16,  -1,  17,  -2,   1,
                -9, -26,  -9, -10,  -2,  -4,   3,  -3,
                -14,   2, -11,  -2,  -5,   2,  14,   5,
                -35,  -8,  11,   2,   8,  15,  -3,   1,
                -1, -18,  -9,  10, -15, -25, -31, -50,
            },
            {
                -65,  23,  16, -15, -56, -34,   2,  13,
                29,  -1, -20,  -7,  -8,  -4, -38, -29,
                -9,  24,   2, -16, -20,   6,  22, -22,
                -17, -20, -12, -27, -30, -25, -14, -36,
                -49,  -1, -27, -39, -46, -44, -33, -51,
                -14, -14, -22, -46, -44, -30, -15, -27,
                1,   7,  -8, -64, -43, -16,   9,   8,
                -15,  36,  12, -54,   8, -28,  24,  14,
            }}};
        
        inline constexpr std::array<std::array<int, 64>, 6> eg_table = {{
            {
                0,   0,   0,   0,   0,   0,   0,   0,
                178, 173, 158, 134, 147, 132, 165, 187,
                94, 100,  85,  67,  56,  53,  82,  84,
                32,  24,  13,   5,  -2,   4,  17,  17,
                13,   9,  -3,  -7,  -7,  -8,   3,  -1,
                4,   7,  -6,   1,   0,  -5,  -1,  -8,
                13,   8,   8,  10,  13,   0,   2,  -7,
                0,   0,   0,   0,   0,   0,   0,   0,
            },
            {
                -58, -38, -13, -28, -31, -27, -63, -99,
                -25,  -8, -25,  -2,  -9, -25, -24, -52,
                -24, -20,  10,   9,  -1,  -9, -19, -41,
                -17,   3,  22,  22,  22,  11,   8, -18,
                -18,  -6,  16,  25,  16,  17,   4, -18,
                -23,  -3,  -1,  15,  10,  -3, -20, -22,
                -42, -20, -10,  -5,  -2, -20, -23, -44,
                -29, -51, -23, -15, -22, -18, -50, -64,
            },
            {
                -14, -21, -11,  -8, -7,  -9, -17, -24,
                -8,  -4,   7, -12, -3, -13,  -4, -14,
                2,  -8,   0,  -1, -2,   6,   0,   4,
                -3,   9,  12,   9, 14,  10,   3,   2,
                -6,   3,  13,  19,  7,  10,  -3,  -9,
                -12,  -3,   8,  10, 13,   3,  -7, -15,
                -14, -18,  -7,  -1,  4,  -9, -15, -27,
                -23,  -9, -23,  -5, -9, -16,  -5, -17,
            },
            {
                13, 10, 18, 15, 12,  12,   8,   5,
                11, 13, 13, 11, -3,   3,   8,   3,
                7,  7,  7,  5,  4,  -3,  -5,  -3,
                4,  3, 13,  1,  2,   1,  -1,   2,
                3,  5,  8,  4, -5,  -6,  -8, -11,
                -4,  0, -5, -1, -7, -12,  -8, -16,
                -6, -6,  0,  2, -9,  -9, -11,  -3,
                -9,  2,  3, -1, -5, -13,   4, -20,
            },
            {
                -9,  22,  22,  27,  27,  19,  10,  20,
                -17,  20,  32,  41,  58,  25,  30,   0,
                -20,   6,   9,  49,  47,  35,  19,   9,
                3,  22,  24,  45,  57,  40,  57,  36,
                -18,  28,  19,  47,  31,  34,  39,  23,
                -16, -27,  15,   6,   9,  17,  10,   5,
                -22, -23, -30, -16, -16, -23, -36, -32,
                -33, -28, -22, -43,  -5, -32, -20, -41,
            },
            {
                -74, -35, -18, -18, -11,  15,   4, -17,
                -12,  17,  14,  17,  17,  38,  23,  11,
                10,  17,  23,  15,  20,  45,  44,  13,
                -8,  22,  24,  27,  26,  33,  26,   3,
                -18,  -4,  21,  24,  27,  23,   9, -11,
                -19,  -3,  11,  21,  23,  16,   7,  -9,
                -27, -11,   4,  13,  14,   4,  -5, -17,
                -53, -34, -21, -11, -28, -14, -24, -43
            }}};
}
