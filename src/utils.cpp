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
