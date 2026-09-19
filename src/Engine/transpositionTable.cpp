#include "Engine/transpositionTable.h"
#include "Engine/settings.h"
#include <algorithm>
using namespace constants;
using std::array;
using random_numbers::random_64_bit_number;
namespace zobrist
{
    array<array<U64, 64>, 12> zobrist_pieces;
    U64 zobrist_side;
    array<U64, 16> zobrist_castle;
    array<U64, 64> zobrist_enpassant;
    
    void init_zobrist_keys()
    {
        static bool initialized = false;
        if (initialized) return;
        initialized = true;
        for (int piece = P; piece <= k; piece++)
        {
            for (int square = a8; square <= h1; square++)
            {
                zobrist_pieces[piece][square] = random_64_bit_number();
            }
        }
        
        for (int castle = 0; castle < 16; castle++)
        {
            zobrist_castle[castle] = random_64_bit_number();
        }

        for (int square = a8; square <= h1; square++)
        {
            zobrist_enpassant[square] = random_64_bit_number();
        }

        zobrist_side = random_64_bit_number();
    }
}


namespace transposition_table {
Table::Table(int megabytes) { resize(megabytes); }
void Table::resize(int megabytes) {
    megabytes = std::clamp(megabytes, 1, tuning::max_hash_mb);
    entries.assign(static_cast<size_t>(megabytes) * 1024 * 1024 / sizeof(Entry), Entry{});
}
void Table::clear() { std::fill(entries.begin(), entries.end(), Entry{}); }
Entry Table::probe(U64 key) const {
    const auto& entry = entries[key % entries.size()];
    return entry.key == key ? entry : Entry{};
}
int Table::score_at_ply(int score, int ply) {
    if (score >= check_mate_score - tuning::max_ply) return score - ply;
    if (score <= -check_mate_score + tuning::max_ply) return score + ply;
    return score;
}
void Table::store(U64 key, unsigned int move, int depth, Bound bound, int score, int ply) {
    if (score >= check_mate_score - tuning::max_ply) score += ply;
    else if (score <= -check_mate_score + tuning::max_ply) score -= ply;
    auto& entry = entries[key % entries.size()];
    if (entry.key == key && entry.depth > depth && bound != exact) return;
    entry = {key, move, score, depth, bound};
}
}
