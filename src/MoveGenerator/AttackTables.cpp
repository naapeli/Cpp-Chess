#include "MoveGenerator/AttackTables.h"
#include "utils.h"
#include <algorithm>
#include <iostream>

using std::fill;
using std::cout;
using std::endl;

using namespace bitboard_utils;
using namespace constants;
using namespace random_numbers;


namespace piece_attacks
{
    U64 bishop_attack_masks(int square)
    {
        U64 attack_table = 0ULL;
        
    
        int row = square / 8;
        int file = square % 8;
        
        for (int r = row + 1, f = file + 1; r <= 6 && f <= 6; r++, f++) attack_table |= (1ULL << (r * 8 + f));
        for (int r = row - 1, f = file + 1; r >= 1 && f <= 6; r--, f++) attack_table |= (1ULL << (r * 8 + f));
        for (int r = row + 1, f = file - 1; r <= 6 && f >= 1; r++, f--) attack_table |= (1ULL << (r * 8 + f));
        for (int r = row - 1, f = file - 1; r >= 1 && f >= 1; r--, f--) attack_table |= (1ULL << (r * 8 + f));
        
        return attack_table;
    }
    
    U64 _bishop_attacks(int square, U64 blockers)
    {
        U64 attack_table = 0ULL;
        
        int row = square / 8;
        int file = square % 8;
        
        for (int r = row + 1, f = file + 1; r <= 7 && f <= 7; r++, f++)
        {
            U64 attack_square = 1ULL << (r * 8 + f);
            attack_table |= attack_square;
            if (attack_square & blockers) break;
        }
        for (int r = row - 1, f = file + 1; r >= 0 && f <= 7; r--, f++)
        {
            U64 attack_square = 1ULL << (r * 8 + f);
            attack_table |= attack_square;
            if (attack_square & blockers) break;
        }
        for (int r = row + 1, f = file - 1; r <= 7 && f >= 0; r++, f--)
        {
            U64 attack_square = 1ULL << (r * 8 + f);
            attack_table |= attack_square;
            if (attack_square & blockers) break;
        }
        for (int r = row - 1, f = file - 1; r >= 0 && f >= 0; r--, f--)
        {
            U64 attack_square = 1ULL << (r * 8 + f);
            attack_table |= attack_square;
            if (attack_square & blockers) break;
        }
        
        return attack_table;
    }

    U64 rook_attack_masks(int square)
    {
        U64 attack_table = 0ULL;
        
    
        int row = square / 8;
        int file = square % 8;
        
        for (int r = row + 1; r <= 6; r++) attack_table |= (1ULL << (r * 8 + file));
        for (int r = row - 1; r >= 1; r--) attack_table |= (1ULL << (r * 8 + file));
        for (int f = file - 1; f >= 1; f--) attack_table |= (1ULL << (row * 8 + f));
        for (int f = file + 1; f <= 6; f++) attack_table |= (1ULL << (row * 8 + f));
        
        return attack_table;
    }
    
    U64 _rook_attacks(int square, U64 blockers)
    {
        U64 attack_table = 0ULL;
        
    
        int row = square / 8;
        int file = square % 8;
        
        for (int r = row + 1; r <= 7; r++)
        {
            U64 attack_square = 1ULL << (r * 8 + file);
            attack_table |= attack_square;
            if (attack_square & blockers) break;
        }
        for (int r = row - 1; r >= 0; r--)
        {
            U64 attack_square = 1ULL << (r * 8 + file);
            attack_table |= attack_square;
            if (attack_square & blockers) break;
        }
        for (int f = file - 1; f >= 0; f--)
        {
            U64 attack_square = 1ULL << (row * 8 + f);
            attack_table |= attack_square;
            if (attack_square & blockers) break;
        }
        for (int f = file + 1; f <= 7; f++)
        {
            U64 attack_square = 1ULL << (row * 8 + f);
            attack_table |= attack_square;
            if (attack_square & blockers) break;
        }
        
        return attack_table;
    }

    U64 _slider_occupancies(int index, U64 attack_mask)
    {
        U64 board = 0ULL;
        int n_bits = count_bits(attack_mask);

        for (int count = 0; count < n_bits; count++)
        {
            int square = least_significant_bit_index(attack_mask);
            pop_bit(attack_mask, square);
            
            if (index & (1 << count))
                board |= (1ULL << square);
        }        
        return board;
    }

    U64 _pawn_attacks(int square, int side)
    {
        U64 attack_table = 0ULL;
        U64 piece_location = 0ULL;
        set_bit(piece_location, square);

        if (side) // black
        {
            if (piece_location & not_a_file & not_1_rank) attack_table |= (piece_location << 7);
            if (piece_location & not_h_file & not_1_rank) attack_table |= (piece_location << 9);
        }
        else // white
        {
            if (piece_location & not_h_file & not_8_rank) attack_table |= (piece_location >> 7);
            if (piece_location & not_a_file & not_8_rank) attack_table |= (piece_location >> 9);
        }
        return attack_table;
    }

    U64 pawn_attacks(int square, int side)
    {
        return pawn_attacks_table[side][square];
    }

    U64 _knight_attacks(int square)
    {
        U64 attack_table = 0ULL;
        U64 piece_location = 0ULL;
        set_bit(piece_location, square);

        if (piece_location & not_ab_file & not_8_rank) attack_table |= (piece_location >> 10);
        if (piece_location & not_a_file & not_78_rank) attack_table |= (piece_location >> 17);
        if (piece_location & not_h_file & not_78_rank) attack_table |= (piece_location >> 15);
        if (piece_location & not_hg_file & not_8_rank) attack_table |= (piece_location >> 6);
        if (piece_location & not_hg_file & not_1_rank) attack_table |= (piece_location << 10);
        if (piece_location & not_h_file & not_12_rank) attack_table |= (piece_location << 17);
        if (piece_location & not_a_file & not_12_rank) attack_table |= (piece_location << 15);
        if (piece_location & not_ab_file & not_1_rank) attack_table |= (piece_location << 6);

        return attack_table;
    }

    U64 knight_attacks(int square)
    {
        return knight_attacks_table[square];
    }

    U64 bishop_attacks(int square, U64 blockers)
    {
        blockers &= bishop_attack_mask_table[square];
        blockers *= magic_numbers::bishop_magic_numbers[square];
        blockers >>= 64 - _bishop_number_of_bits_in_attack_mask[square];
        return bishop_attacks_table[square][blockers];
    }

    U64 rook_attacks(int square, U64 blockers)
    {
        blockers &= rook_attack_mask_table[square];
        blockers *= magic_numbers::rook_magic_numbers[square];
        blockers >>= 64 - _rook_number_of_bits_in_attack_mask[square];
        return rook_attacks_table[square][blockers];
    }

    U64 queen_attacks(int square, U64 blockers)
    {
        return bishop_attacks(square, blockers) | rook_attacks(square, blockers);
    }

    U64 _king_attacks(int square)
    {
        U64 attack_table = 0ULL;
        U64 piece_location = 0ULL;
        set_bit(piece_location, square);

        if (piece_location & not_a_file & not_8_rank) attack_table |= (piece_location >> 9);
        if (piece_location & not_8_rank) attack_table |= (piece_location >> 8);
        if (piece_location & not_h_file & not_8_rank) attack_table |= (piece_location >> 7);
        if (piece_location & not_h_file) attack_table |= (piece_location << 1);
        if (piece_location & not_h_file & not_1_rank) attack_table |= (piece_location << 9);
        if (piece_location & not_1_rank) attack_table |= (piece_location << 8);
        if (piece_location & not_a_file & not_1_rank) attack_table |= (piece_location << 7);
        if (piece_location & not_a_file) attack_table |= (piece_location >> 1);

        return attack_table;
    }

    U64 king_attacks(int square)
    {
        return king_attacks_table[square];
    }

    U64 bishop_attack_mask_table[64];
    U64 rook_attack_mask_table[64];

    U64 pawn_attacks_table[2][64];
    U64 knight_attacks_table[64];
    U64 bishop_attacks_table[64][512];
    U64 rook_attacks_table[64][4096];
    U64 king_attacks_table[64];

    void init_leaper_attacks()
    {
        for (int square = 0; square < 64; square++)
        {
            pawn_attacks_table[white][square] = _pawn_attacks(square, white);
            pawn_attacks_table[black][square] = _pawn_attacks(square, black);
            knight_attacks_table[square] = _knight_attacks(square);
            king_attacks_table[square] = _king_attacks(square);
        }
    }

    void init_slider_attacks()
    {
        for (int square = 0; square < 64; square++)
        {
            bishop_attack_mask_table[square] = bishop_attack_masks(square);
            rook_attack_mask_table[square] = rook_attack_masks(square);

            U64 bishop_mask = bishop_attack_mask_table[square];
            U64 rook_mask = rook_attack_mask_table[square];
            int n_bits_bishop = count_bits(bishop_mask);
            int n_bits_rook = count_bits(rook_mask);
            int bishop_occupancy_indicies = (1 << n_bits_bishop);
            int rook_occupancy_indicies = (1 << n_bits_rook);


            for (int index = 0; index < bishop_occupancy_indicies; index++)
            {
                U64 blockers = _slider_occupancies(index, bishop_mask);
                int magic_index = (blockers * magic_numbers::bishop_magic_numbers[square]) >> (64 - _bishop_number_of_bits_in_attack_mask[square]);
                bishop_attacks_table[square][magic_index] = _bishop_attacks(square, blockers);
            }

            for (int index = 0; index < rook_occupancy_indicies; index++)
            {
                U64 blockers = _slider_occupancies(index, rook_mask);
                int magic_index = (blockers * magic_numbers::rook_magic_numbers[square]) >> (64 - _rook_number_of_bits_in_attack_mask[square]);
                rook_attacks_table[square][magic_index] = _rook_attacks(square, blockers);
            }
        }
    }

    array<array<U64, 64>, 64> align_mask;
    void _init_align_masks()
    {
        for (int king_location = 0; king_location < 64; king_location++)
        {
            int row = king_location / 8;
            int file = king_location % 8;
            
            U64 mask = 0ULL;
            for (int r = row + 1, f = file + 1; r <= 7 && f <= 7; r++, f++)
            {
                set_bit(mask, r * 8 + f);
            }
            for (int r = row + 1, f = file + 1; r <= 7 && f <= 7; r++, f++)
            {
                align_mask[r * 8 + f][king_location] = mask;
            }

            mask = 0ULL;
            for (int r = row - 1, f = file + 1; r >= 0 && f <= 7; r--, f++)
            {
                set_bit(mask, r * 8 + f);
            }
            for (int r = row - 1, f = file + 1; r >= 0 && f <= 7; r--, f++)
            {
                align_mask[r * 8 + f][king_location] = mask;
            }

            mask = 0ULL;
            for (int r = row + 1, f = file - 1; r <= 7 && f >= 0; r++, f--)
            {
                set_bit(mask, r * 8 + f);
            }
            for (int r = row + 1, f = file - 1; r <= 7 && f >= 0; r++, f--)
            {
                align_mask[r * 8 + f][king_location] = mask;
            }

            mask = 0ULL;
            for (int r = row - 1, f = file - 1; r >= 0 && f >= 0; r--, f--)
            {
                set_bit(mask, r * 8 + f);
            }
            for (int r = row - 1, f = file - 1; r >= 0 && f >= 0; r--, f--)
            {
                align_mask[r * 8 + f][king_location] = mask;
            }
        }
    }

    void init_all_attacks()
    {
        init_leaper_attacks();
        init_slider_attacks();
    }
}

namespace magic_numbers
{
    // U64 rook_magic_numbers[64];
    // U64 bishop_magic_numbers[64];

    U64 rook_magic_numbers[64] = {
        0x8a80104000800020ULL,
        0x140002000100040ULL,
        0x2801880a0017001ULL,
        0x100081001000420ULL,
        0x200020010080420ULL,
        0x3001c0002010008ULL,
        0x8480008002000100ULL,
        0x2080088004402900ULL,
        0x800098204000ULL,
        0x2024401000200040ULL,
        0x100802000801000ULL,
        0x120800800801000ULL,
        0x208808088000400ULL,
        0x2802200800400ULL,
        0x2200800100020080ULL,
        0x801000060821100ULL,
        0x80044006422000ULL,
        0x100808020004000ULL,
        0x12108a0010204200ULL,
        0x140848010000802ULL,
        0x481828014002800ULL,
        0x8094004002004100ULL,
        0x4010040010010802ULL,
        0x20008806104ULL,
        0x100400080208000ULL,
        0x2040002120081000ULL,
        0x21200680100081ULL,
        0x20100080080080ULL,
        0x2000a00200410ULL,
        0x20080800400ULL,
        0x80088400100102ULL,
        0x80004600042881ULL,
        0x4040008040800020ULL,
        0x440003000200801ULL,
        0x4200011004500ULL,
        0x188020010100100ULL,
        0x14800401802800ULL,
        0x2080040080800200ULL,
        0x124080204001001ULL,
        0x200046502000484ULL,
        0x480400080088020ULL,
        0x1000422010034000ULL,
        0x30200100110040ULL,
        0x100021010009ULL,
        0x2002080100110004ULL,
        0x202008004008002ULL,
        0x20020004010100ULL,
        0x2048440040820001ULL,
        0x101002200408200ULL,
        0x40802000401080ULL,
        0x4008142004410100ULL,
        0x2060820c0120200ULL,
        0x1001004080100ULL,
        0x20c020080040080ULL,
        0x2935610830022400ULL,
        0x44440041009200ULL,
        0x280001040802101ULL,
        0x2100190040002085ULL,
        0x80c0084100102001ULL,
        0x4024081001000421ULL,
        0x20030a0244872ULL,
        0x12001008414402ULL,
        0x2006104900a0804ULL,
        0x1004081002402ULL
    };

    U64 bishop_magic_numbers[64] = {
        0x40040844404084ULL,
        0x2004208a004208ULL,
        0x10190041080202ULL,
        0x108060845042010ULL,
        0x581104180800210ULL,
        0x2112080446200010ULL,
        0x1080820820060210ULL,
        0x3c0808410220200ULL,
        0x4050404440404ULL,
        0x21001420088ULL,
        0x24d0080801082102ULL,
        0x1020a0a020400ULL,
        0x40308200402ULL,
        0x4011002100800ULL,
        0x401484104104005ULL,
        0x801010402020200ULL,
        0x400210c3880100ULL,
        0x404022024108200ULL,
        0x810018200204102ULL,
        0x4002801a02003ULL,
        0x85040820080400ULL,
        0x810102c808880400ULL,
        0xe900410884800ULL,
        0x8002020480840102ULL,
        0x220200865090201ULL,
        0x2010100a02021202ULL,
        0x152048408022401ULL,
        0x20080002081110ULL,
        0x4001001021004000ULL,
        0x800040400a011002ULL,
        0xe4004081011002ULL,
        0x1c004001012080ULL,
        0x8004200962a00220ULL,
        0x8422100208500202ULL,
        0x2000402200300c08ULL,
        0x8646020080080080ULL,
        0x80020a0200100808ULL,
        0x2010004880111000ULL,
        0x623000a080011400ULL,
        0x42008c0340209202ULL,
        0x209188240001000ULL,
        0x400408a884001800ULL,
        0x110400a6080400ULL,
        0x1840060a44020800ULL,
        0x90080104000041ULL,
        0x201011000808101ULL,
        0x1a2208080504f080ULL,
        0x8012020600211212ULL,
        0x500861011240000ULL,
        0x180806108200800ULL,
        0x4000020e01040044ULL,
        0x300000261044000aULL,
        0x802241102020002ULL,
        0x20906061210001ULL,
        0x5a84841004010310ULL,
        0x4010801011c04ULL,
        0xa010109502200ULL,
        0x4a02012000ULL,
        0x500201010098b028ULL,
        0x8040002811040900ULL,
        0x28000010020204ULL,
        0x6000020202d0240ULL,
        0x8918844842082200ULL,
        0x4010011029020020ULL
    };

    U64 _find_magic(int square, int piece)
    {
        U64 occupancies[4096];    
        U64 true_attacks[4096];    
        U64 used_attacks[4096];
        
        U64 attack_mask = piece ? piece_attacks::bishop_attack_masks(square) : piece_attacks::rook_attack_masks(square);
        int n_relevant_bits = count_bits(attack_mask);
        int occupancy_indicies = 1 << n_relevant_bits;
        for (int index = 0; index < occupancy_indicies; index++)
        {
            occupancies[index] = piece_attacks::_slider_occupancies(index, attack_mask);
            true_attacks[index] = piece ? piece_attacks::_bishop_attacks(square, occupancies[index]) :
                                        piece_attacks::_rook_attacks(square, occupancies[index]);
        }

        for (int random_count = 0; random_count < 100000000; random_count++)
        {
            U64 magic_number = random_magic_number();            
            if (count_bits((attack_mask * magic_number) & 0xFF00000000000000) < 6) continue;
            fill(used_attacks, used_attacks + 4096, 0ULL);

            int index;
            int fail = 0;
            for (index = 0; !fail && index < occupancy_indicies; index++)
            {
                int magic_index = (int)((occupancies[index] * magic_number) >> (64 - n_relevant_bits));
                if (used_attacks[magic_index] == 0ULL)
                    used_attacks[magic_index] = true_attacks[index];                
                else if (used_attacks[magic_index] != true_attacks[index])
                    fail = 1;
            }
            if (!fail)
                return magic_number;
        }
        return 0ULL;
    }

    void _initialize_magic_numbers()
    {
        for (int square = 0; square < 64; square++)
            rook_magic_numbers[square] = _find_magic(square, 0);

        for (int square = 0; square < 64; square++)
            bishop_magic_numbers[square] = _find_magic(square, 1);
    }
}
