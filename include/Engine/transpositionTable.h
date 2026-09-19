#pragma once
#include "utils.h"
#include <array>
#include <vector>

namespace zobrist {
extern std::array<std::array<U64, 64>, 12> zobrist_pieces;
extern U64 zobrist_side;
extern std::array<U64, 16> zobrist_castle;
extern std::array<U64, 64> zobrist_enpassant;
void init_zobrist_keys();
}

namespace transposition_table {
enum Bound { exact, lowerbound, upperbound };
struct Entry {
    U64 key = 0;
    unsigned int move = 0;
    int score = 0;
    int depth = -1;
    Bound bound = upperbound;
};
class Table {
public:
    explicit Table(int megabytes);
    void resize(int megabytes);
    void clear();
    Entry probe(U64 key) const;
    void store(U64 key, unsigned int move, int depth, Bound bound, int score, int ply);
    static int score_at_ply(int score, int ply);
private:
    std::vector<Entry> entries;
};
}
