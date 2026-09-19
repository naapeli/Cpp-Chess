#include "Engine/engine.h"
#include "Engine/evaluation.h"
#include "MoveGenerator/AttackTables.h"
#include "MoveGenerator/MoveGenerator.h"
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace constants;
using namespace bitboard_utils;
using board_utils::parse_fen;

int checks = 0;
void require(bool condition, const std::string& message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
const char* start_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

void verify_tree(board::board_state position, int depth) {
    require(position.zobrist_hash == board_utils::get_zobrist_hash(position), "incremental hash matches recomputed hash");
    U64 white_pieces = 0, black_pieces = 0;
    for (int piece = P; piece <= k; ++piece) (piece < 6 ? white_pieces : black_pieces) |= position.bitboards[piece];
    require(white_pieces == position.occupancies[white] && black_pieces == position.occupancies[black] &&
        (white_pieces | black_pieces) == position.occupancies[both] && !(white_pieces & black_pieces), "occupancy consistency");
    if (!depth) return;
    std::array<unsigned int, max_moves> buffer;
    std::set<unsigned int> seen;
    for (auto move : move_generator::generate_moves(position, buffer, false)) {
        require(seen.insert(move).second, "no duplicate moves");
        require(board::encode_move(position, board::move_to_string(move)) == move, "UCI round trip");
        auto next = board::make_move(position, move);
        auto previous_side = next; previous_side.side = position.side;
        require(!move_generator::is_square_attacked(least_significant_bit_index(next.bitboards[position.side == white ? K : k]), previous_side), "move leaves own king safe");
        verify_tree(next, depth - 1);
    }
}
void check_pv(board::board_state position, const SearchResult& result) {
    for (auto move : result.pv) {
        require(board::encode_move(position, board::move_to_string(move)) == move, "legal PV");
        position = board::make_move(position, move);
    }
}
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--legal-moves") {
        piece_attacks::init_all();
        std::string fen;
        while (std::getline(std::cin, fen)) {
            try {
                auto position = parse_fen(fen);
                std::array<unsigned int, max_moves> buffer;
                for (auto move : move_generator::generate_moves(position, buffer, false))
                    std::cout << board::move_to_string(move) << ' ';
                std::cout << std::endl;
            } catch (const std::exception& error) { std::cout << "ERROR " << error.what() << std::endl; }
        }
        return 0;
    }
    try {
        piece_attacks::init_all();
        auto initial = parse_fen(start_fen);
        const U64 original_hash = initial.zobrist_hash;
        piece_attacks::init_all();
        require(parse_fen(start_fen).zobrist_hash == original_hash, "idempotent initialization");
        std::set<U64> keys;
        for (const auto& piece : zobrist::zobrist_pieces) for (auto key : piece) require(key && keys.insert(key).second, "unique piece-square hash keys");
        struct Perft { const char* fen; int depth; int nodes; };
        const Perft positions[] = {
            {start_fen, 4, 197281},
            {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862},
            {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238},
            {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467},
            {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379},
            {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 3, 89890},
        };
        for (const auto& item : positions) {
            auto position = parse_fen(item.fen);
            require(move_generator::perft(position, item.depth) == item.nodes, std::string("perft: ") + item.fen);
            verify_tree(position, 2);
        }
        for (auto fen : {"5b1k/8/8/3pP3/1K6/8/8/8 w - d6 0 1", "7k/5b2/8/3pP3/2K5/8/8/8 w - d6 0 1", "7k/8/8/r4pPK/8/8/8/8 w - f6 0 1"}) {
            auto position = parse_fen(fen); verify_tree(position, 2);
        }
        auto ep = parse_fen("5b1k/8/8/3pP3/1K6/8/8/8 w - d6 0 1");
        require(board::encode_move(ep, "e5d6") != invalid_move, "EP can block diagonal check");
        auto next = board::make_move(ep, board::encode_move(ep, "e5d6"));
        require(next.bitboards[p] == 0 && get_bit(next.bitboards[P], d6), "EP removes captured pawn");
        auto pinned_ep = parse_fen("7k/5b2/8/3pP3/2K5/8/8/8 w - d6 0 1");
        require(!board::encode_move(pinned_ep, "e5d6"), "EP cannot uncover bishop check");
        auto no_ep = pinned_ep; no_ep.enpassant = no_square; no_ep.zobrist_hash = board_utils::get_zobrist_hash(no_ep);
        require(board_utils::repetition_key(pinned_ep) == board_utils::repetition_key(no_ep), "illegal EP irrelevant to repetition");
        auto null_position = board::make_null_move(ep);
        require(null_position.side != ep.side && null_position.enpassant == no_square && null_position.zobrist_hash == board_utils::get_zobrist_hash(null_position), "null-move hash");
        auto castle = parse_fen("r3k2r/6B1/8/8/8/8/8/4K3 w kq - 19 24");
        castle = board::make_move(castle, board::encode_move(castle, "g7h8"));
        require(!(castle.castle & bk) && (castle.castle & bq) && castle.halfmove_clock == 0 && castle.fullmove_number == 24, "rook capture removes castling rights and resets clock");
        require(castle.zobrist_hash == board_utils::get_zobrist_hash(castle), "rook capture hash");
        auto clock_position = board::make_move(initial, board::encode_move(initial, "g1f3"));
        clock_position = board::make_move(clock_position, board::encode_move(clock_position, "g8f6"));
        require(clock_position.halfmove_clock == 2 && clock_position.fullmove_number == 2, "move counters");
        auto promotion = parse_fen("7k/P7/8/8/8/8/8/7K w - - 0 1");
        require(board::move_to_string(board::encode_move(promotion, "a7a8n")) == "a7a8n", "lowercase promotion notation");
        require(board::move_to_string(invalid_move) == "0000" && board::move_to_string(board::encode_move(initial, "e2e4")) == "e2e4", "UCI move notation");
        for (auto fen : {"", "8/8/8/8/8/8/8/8 w - -", "7k/8/8/8/8/8/8/7K x - -", "7k/8/8/8/8/8/8/7K w - z9", "7k/8/8/8/8/8/8/7K w - - -1 1", "7k/8/8/8/8/8/8/7K w - - 0"}) {
            bool rejected = false;
            try { parse_fen(fen); } catch (const std::exception&) { rejected = true; }
            require(rejected, "malformed FEN rejected");
        }
        require(evaluation::evaluate(initial) == 0, "symmetric starting evaluation");
        auto advantage = parse_fen("7k/8/8/8/8/3Q4/8/K7 w - - 0 1");
        require(evaluation::evaluate(advantage) > 500, "material advantage sign");
        auto other_turn = advantage; other_turn.side = !other_turn.side;
        require(evaluation::evaluate(advantage) == -evaluation::evaluate(other_turn), "side-to-move antisymmetry");
        auto mirrored = advantage;
        std::fill(std::begin(mirrored.bitboards), std::end(mirrored.bitboards), 0ULL);
        for (int piece = P; piece <= k; ++piece) for (int sq = 0; sq < 64; ++sq)
            if (get_bit(advantage.bitboards[piece], sq)) set_bit(mirrored.bitboards[(piece + 6) % 12], sq ^ 56);
        mirrored.side = !advantage.side;
        require(evaluation::evaluate(mirrored) == evaluation::evaluate(advantage), "color-mirror evaluation symmetry");
        require(evaluation::evaluate(parse_fen("7k/8/8/8/8/8/8/KB6 w - - 0 1")) == 0, "insufficient material");
        transposition_table::Table table(1);
        table.store(123, 42, 5, transposition_table::exact, check_mate_score - 5, 3);
        require(transposition_table::Table::score_at_ply(table.probe(123).score, 7) == check_mate_score - 9, "TT mate distance normalization");
        require(table.probe(124).depth == -1, "TT miss");
        const U64 collision = 123 + 1024 * 1024 / sizeof(transposition_table::Entry);
        require(table.probe(collision).depth == -1, "TT collision must not return another position's move");
        table.clear(); require(table.probe(123).depth == -1, "TT clear");

        // Depth one without iterative deepening cannot create a TT hit within
        // the same search. A subsequent root hit proves cross-search retention.
        {
            EngineSettings retained_settings;
            retained_settings.iterative_deepening = false;
            Engine retained(retained_settings);
            SearchLimits shallow; shallow.depth = 1;
            auto cold = retained.search(initial, shallow);
            auto warm = retained.search(initial, shallow);
            require(cold.statistics.transposition_table == 0 && warm.statistics.transposition_table > 0,
                    "TT survives between searches");
            require(cold.score == warm.score && cold.best_move == warm.best_move, "warm TT preserves shallow result");
            retained.clear_hash();
            require(retained.search(initial, shallow).statistics.transposition_table == 0, "explicit clear empties TT");
            retained.configure(retained.settings());
            require(retained.search(initial, shallow).statistics.transposition_table > 0, "unchanged options retain TT");
            retained.clear_hash();
            SearchLimits two_plies; two_plies.depth = 2;
            auto previous_move = retained.search(initial, two_plies);
            auto reply_position = board::make_move(initial, previous_move.best_move);
            retained.set_history({board_utils::repetition_key(initial), board_utils::repetition_key(reply_position)});
            require(retained.search(reply_position, shallow).statistics.transposition_table > 0,
                    "TT entries survive advancing the game with its history");
        }

        EngineSettings settings;
        settings.check_extensions = settings.pawn_extensions = settings.forced_move_extensions = false;
        Engine engine(settings);
        SearchLimits limits; limits.depth = 3;
        auto result = engine.search(initial, limits);
        require(result.depth == 3 && result.best_move, "completed depth search"); check_pv(initial, result);
        engine.clear_hash();
        auto repeat = engine.search(initial, limits);
        require(repeat.score == result.score && repeat.best_move == result.best_move && repeat.nodes == result.nodes, "deterministic fixed-depth search");
        // Exact search optimizations must agree with plain alpha-beta.
        settings.null_move_pruning = settings.late_move_reductions = settings.delta_pruning = false;
        engine.configure(settings);
        auto optimized = engine.search(initial, limits);
        settings.transposition_table = settings.principal_variation_search = settings.aspiration_windows = false;
        settings.killer_ordering = settings.history_ordering = false;
        engine.configure(settings);
        auto reference = engine.search(initial, limits);
        require(reference.score == optimized.score, "plain alpha-beta agrees with exact optimizations");
        limits.depth = 1;
        auto mate_in_one = parse_fen("7k/5Q2/6K1/8/8/8/8/8 w - - 0 1");
        result = engine.search(mate_in_one, limits);
        require(result.score == check_mate_score - 1, "mate at quiescence horizon"); check_pv(mate_in_one, result);
        auto before_stalemate = parse_fen("7k/8/6K1/5Q2/8/8/8/8 w - - 0 1");
        auto horizon = limits; horizon.restrict_root_moves = true;
        horizon.root_moves = {board::encode_move(before_stalemate, "f5f7")};
        require(horizon.root_moves.front() != invalid_move, "stalemate setup is legal");
        require(engine.search(before_stalemate, horizon).score == 0, "stalemate at quiescence horizon");
        auto no_q = engine.settings(); no_q.quiescence = false; engine.configure(no_q);
        require(engine.search(mate_in_one, limits).score == check_mate_score - 1, "terminal detection with quiescence disabled");
        auto mate = parse_fen("7k/6Q1/6K1/8/8/8/8/8 b - - 100 1");
        result = engine.search(mate, limits);
        require(result.score == -check_mate_score && result.best_move == invalid_move, "checkmate precedes fifty-move draw");
        auto stalemate = parse_fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
        result = engine.search(stalemate, limits);
        require(result.score == 0 && result.best_move == invalid_move, "stalemate");
        auto fifty = advantage; fifty.halfmove_clock = 100;
        require(engine.search(fifty, limits).score == 0, "fifty-move draw");
        auto cycle = initial;
        std::vector<U64> history{board_utils::repetition_key(cycle)};
        for (int n = 0; n < 2; ++n) for (auto text : {"g1f3", "g8f6", "f3g1", "f6g8"}) {
            cycle = board::make_move(cycle, board::encode_move(cycle, text));
            history.push_back(board_utils::repetition_key(cycle));
        }
        engine.set_history(history);
        require(engine.search(cycle, limits).score == 0, "threefold draw with supplied history");
        engine.clear_repetition_table();
        // Two occurrences alone must not be treated as a threefold claim.
        auto repeated_advantage = advantage; repeated_advantage.halfmove_clock = 10;
        engine.set_history({board_utils::repetition_key(advantage), board_utils::repetition_key(advantage)});
        require(engine.search(repeated_advantage, limits).score > 500, "twofold is not threefold");
        engine.clear_repetition_table();
        for (const auto& option : boolean_options) {
            auto toggled = EngineSettings{}; toggled.*option.member = false;
            engine.configure(toggled);
            limits.depth = 2;
            result = engine.search(initial, limits);
            require(result.depth == 2 && board::encode_move(initial, board::move_to_string(result.best_move)), std::string("option off: ") + std::string(option.name));
            check_pv(initial, result);
            require(result.statistics.*option.events == 0, std::string("disabled technique has zero events: ") + std::string(option.name));
        }
        // Compare exhaustive negamax against alpha-beta over the same horizon.
        EngineSettings plain;
        for (const auto& option : boolean_options) plain.*option.member = false;
        limits.depth = 3;
        engine.configure(plain);
        auto exhaustive = engine.search(initial, limits);
        require(exhaustive.statistics.alpha_beta_pruning == 0, "exhaustive mode has no beta cutoffs");
        plain.alpha_beta_pruning = true;
        engine.configure(plain);
        auto bounded = engine.search(initial, limits);
        require(exhaustive.score == bounded.score && exhaustive.nodes > bounded.nodes, "alpha-beta preserves exhaustive score while reducing nodes");

        // Exercise delta pruning in quiescence, not just its UCI advertisement.
        auto tactical = parse_fen(positions[1].fen);
        auto delta_settings = EngineSettings{};
        engine.configure(delta_settings);
        auto delta_on = engine.search(tactical, limits);
        require(delta_on.statistics.delta_pruning > 0, "delta pruning executes in tactical quiescence");
        delta_settings.delta_pruning = false;
        engine.configure(delta_settings);
        auto delta_off = engine.search(tactical, limits);
        require(delta_off.statistics.delta_pruning == 0, "delta switch disables quiescence pruning");
        check_pv(tactical, delta_on); check_pv(tactical, delta_off);

        // Null probes must leave legal history and the caller's board intact.
        engine.configure(EngineSettings{});
        limits.depth = 4;
        engine.set_history({board_utils::repetition_key(initial)});
        auto null_on = engine.search(initial, limits);
        require(null_on.statistics.null_probes > 0 && null_on.statistics.null_move_pruning > 0, "null-move probe and cutoff exercised");
        engine.clear_hash();
        auto after_null = engine.search(initial, limits);
        require(after_null.score == null_on.score && after_null.nodes == null_on.nodes && initial.zobrist_hash == original_hash, "null probe does not contaminate history or board");
        auto null_settings = EngineSettings{}; null_settings.null_move_pruning = false;
        engine.configure(null_settings);
        auto null_off = engine.search(initial, limits);
        require(null_off.statistics.null_probes == 0 && null_off.statistics.null_move_pruning == 0, "null switch disables probes");
        engine.configure(EngineSettings{});
        auto pawn_ending = parse_fen("8/5k2/5p2/4pP2/4P3/5K2/8/8 w - - 0 1");
        require(engine.search(pawn_ending, limits).statistics.null_probes == 0, "null move excluded in pawn-only endings");
        engine.clear_repetition_table();

        engine.configure(EngineSettings{});
        std::atomic_bool stop{false};
        limits.depth = 10;
        SearchResult completed;
        result = engine.search(initial, limits, &stop, [&](const SearchResult& iteration) { completed = iteration; stop = true; });
        require(result.depth == 1 && result.best_move == completed.best_move && result.score == completed.score && result.pv == completed.pv, "stop preserves completed iteration");
        result = engine.search(initial, limits, &stop);
        require(result.depth == 0 && result.best_move != invalid_move, "immediate stop has legal fallback");
        limits.nodes = 1;
        result = engine.search(initial, limits);
        require(result.nodes <= 1 && result.best_move != invalid_move, "node budget");
        limits.nodes = 0; limits.depth = 2; limits.restrict_root_moves = true;
        limits.root_moves = {board::encode_move(initial, "a2a3")};
        require(engine.search(initial, limits).best_move == limits.root_moves.front(), "restricted root move");
        std::cout << checks << " correctness checks passed\n";
    } catch (const std::exception& error) { std::cerr << "After " << checks << " checks: " << error.what() << '\n'; return 1; }
}
