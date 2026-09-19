#include "Engine/evaluation.h"
#include "Engine/settings.h"
#include <algorithm>
using namespace constants;
using namespace bitboard_utils;

namespace evaluation {
bool insufficient_material(const board::board_state& board) {
    if (board.bitboards[P] | board.bitboards[p] | board.bitboards[R] |
        board.bitboards[r] | board.bitboards[Q] | board.bitboards[q]) return false;
    U64 bishops = board.bitboards[B] | board.bitboards[b];
    const int knights = count_bits(board.bitboards[N] | board.bitboards[n]);
    if (count_bits(bishops) + knights <= 1) return true;
    if (knights) return false;
    // Any number of bishops confined to one square color cannot mate.
    bool light = false, dark = false;
    while (bishops) {
        int sq = least_significant_bit_index(bishops);
        ((sq / 8 + sq % 8) % 2 ? light : dark) = true;
        pop_bit(bishops, sq);
    }
    return !(light && dark);
}
int evaluate(const board::board_state& board) {
    if (insufficient_material(board)) return 0;
    int mg = 0, eg = 0, phase = 0;
    for (int piece = P; piece <= k; ++piece) {
        U64 bits = board.bitboards[piece];
        const int type = piece % 6;
        const int sign = piece < 6 ? 1 : -1;
        while (bits) {
            int square = least_significant_bit_index(bits);
            const int oriented = piece < 6 ? square : square ^ 56;
            mg += sign * (tuning::mg_material[type] + tuning::mg_table[type][oriented]);
            eg += sign * (tuning::eg_material[type] + tuning::eg_table[type][oriented]);
            phase += tuning::phase_weights[type];
            pop_bit(bits, square);
        }
    }
    phase = std::min(phase, tuning::total_phase);
    int value = (mg * phase + eg * (tuning::total_phase - phase)) / tuning::total_phase;
    return board.side == white ? value : -value;
}
}
