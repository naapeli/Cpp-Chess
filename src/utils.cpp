#include <iostream>
#include "utils.h"

using namespace std;
using namespace constants;
using namespace bitboard_utils;


namespace bitboard_utils
{
    void set_bit(U64 &bitboard, int square)
    {
        bitboard |= (1ULL << square);
    }

    int get_bit(U64 bitboard, int square)
    {
        return ((bitboard >> square) & 1);
    }

    int pop_bit(U64 &bitboard, int square)
    {
        int bit = get_bit(bitboard, square);
        if (bit)
        {
            bitboard ^= 1ULL << square;
        }
        return bit;
    }

    U64 shift(U64 bitboard, int amount)
    {
        if (amount < 0)
        {
            return bitboard << -amount;
        }
        return bitboard >> amount;
    }

    int count_bits(U64 bitboard)
    {
        int count = 0;
        while (bitboard)
        {
            count++;
            bitboard &= bitboard - 1;
        }
        return count;
    }

    int least_significant_bit_index(U64 bitboard)
    {
        if (bitboard)
        {
            return count_bits((bitboard & -bitboard) - 1);
        }
        else
            return -1;
    }

    void print_bitboard(U64 bitboard)
    {
        for (int rank = 0; rank < 8; rank++)
        {
            cout << 8 - rank << "   ";
            for (int file = 0; file < 8; file++)
            {
                int square = 8 * rank + file;
                int bit = get_bit(bitboard, square);
                cout << bit << " ";
            }
            cout << endl;
        }
        cout << endl;
        cout << "    " << "a b c d e f g h" << endl << endl << endl;
    }
}

namespace random_numbers
{
    unsigned int state = 1804289383;

    unsigned int random_32_bit_number()
    {
        unsigned int number = state;

        number ^= number << 13;
        number ^= number >> 17;
        number ^= number << 5;

        state = number;
        return state;
    }

    U64 random_64_bit_number()
    {
        U64 n1 = (U64)(random_32_bit_number() & 0xFFFF);
        U64 n2 = (U64)(random_32_bit_number() & 0xFFFF);
        U64 n3 = (U64)(random_32_bit_number() & 0xFFFF);
        U64 n4 = (U64)(random_32_bit_number() & 0xFFFF);

        return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
    }

    U64 random_magic_number()
    {
        return random_64_bit_number() & random_64_bit_number() & random_64_bit_number();
    }
}

namespace constants
{
    const std::string square_to_coordinates[64] = {
        "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
        "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
        "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
        "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
        "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
        "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
        "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
        "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
    };
    const std::string piece_to_string = "PNBRQKpnbrqk";
    const std::string promotion_to_string = " K B   R       Q";
    const std::unordered_map<char, int> string_to_piece = {{'P', P}, {'N', N}, {'B', B}, {'R', R}, {'Q', Q}, {'K', K},
                                                     {'p', p}, {'n', n}, {'b', b}, {'r', r}, {'q', q}, {'k', k}};
}
