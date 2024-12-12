#ifndef attacks
#define attacks

#include "utils.h"

namespace piece_attacks
{
    const U64 not_a_file = 18374403900871474942ULL;
    const U64 not_h_file = 9187201950435737471ULL;
    const U64 not_ab_file = 18229723555195321596ULL;
    const U64 not_hg_file = 4557430888798830399ULL;
    const U64 not_1_rank = 0x00FFFFFFFFFFFFFFULL;
    const U64 not_12_rank = 0x0000FFFFFFFFFFFFULL;
    const U64 not_8_rank = 0xFFFFFFFFFFFFFF00ULL;
    const U64 not_78_rank = 0xFFFFFFFFFFFF0000ULL;

    extern U64 bishop_attack_mask_table[64];
    extern U64 rook_attack_mask_table[64];

    extern U64 pawn_attacks_table[2][64];
    extern U64 knight_attacks_table[64];
    extern U64 bishop_attacks_table[64][512];
    extern U64 rook_attacks_table[64][4096];
    extern U64 king_attacks_table[64];

    U64 bishop_attack_masks(int square);
    U64 _bishop_attacks(int square, U64 blockers);
    U64 rook_attack_masks(int square);
    U64 _rook_attacks(int square, U64 blockers);

    const int _bishop_number_of_bits_in_attack_mask[64] = {
        6, 5, 5, 5, 5, 5, 5, 6,
        5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 7, 7, 7, 7, 5, 5,
        5, 5, 7, 9, 9, 7, 5, 5,
        5, 5, 7, 9, 9, 7, 5, 5,
        5, 5, 7, 7, 7, 7, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5,
        6, 5, 5, 5, 5, 5, 5, 6
    };

    const int _rook_number_of_bits_in_attack_mask[64] = {
        12, 11, 11, 11, 11, 11, 11, 12,
        11, 10, 10, 10, 10, 10, 10, 11,
        11, 10, 10, 10, 10, 10, 10, 11,
        11, 10, 10, 10, 10, 10, 10, 11,
        11, 10, 10, 10, 10, 10, 10, 11,
        11, 10, 10, 10, 10, 10, 10, 11,
        11, 10, 10, 10, 10, 10, 10, 11,
        12, 11, 11, 11, 11, 11, 11, 12
    };

    U64 _slider_occupancies(int index, U64 attack_mask);

    U64 pawn_attacks(int square, int side);
    U64 knight_attacks(int square);
    U64 bishop_attacks(int square, U64 blockers);
    U64 rook_attacks(int square, U64 blockers);
    U64 queen_attacks(int square, U64 blockers);
    U64 king_attacks(int square);

    void init_leaper_attacks();
    void init_slider_attacks();
    void init_all_attacks();
}

namespace magic_numbers
{
    extern U64 rook_magic_numbers[64];
    extern U64 bishop_magic_numbers[64];

    U64 _find_magic(int square, int piece);

    void _initialize_magic_numbers();
}

#endif
