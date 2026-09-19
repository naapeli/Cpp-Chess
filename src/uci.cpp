#include "uci.h"
#include "Engine/engine.h"
#include "MoveGenerator/AttackTables.h"
#include <algorithm>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {
const std::string start_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
std::mutex output_mutex;
void send(const std::string& text) {
    std::lock_guard lock(output_mutex);
    std::cout << text << std::endl;
}
int number(const std::string& token, int minimum = 0, int maximum = 1000000000) {
    size_t end = 0;
    long long value = std::stoll(token, &end);
    if (end != token.size() || value < minimum || value > maximum) throw std::invalid_argument("invalid numeric value");
    return static_cast<int>(value);
}
void report(const SearchResult& result) {
    std::ostringstream out;
    out << "info depth " << result.depth << " score ";
    if (std::abs(result.score) >= constants::check_mate_score - tuning::max_ply) {
        int distance = constants::check_mate_score - std::abs(result.score);
        out << "mate " << (result.score < 0 ? -1 : 1) * ((distance + 1) / 2);
    } else out << "cp " << result.score;
    out << " nodes " << result.nodes << " time " << result.elapsed_ms
        << " nps " << result.nodes * 1000 / std::max(1LL, result.elapsed_ms) << " pv";
    for (auto move : result.pv) out << ' ' << board::move_to_string(move);
    send(out.str());
}
void report_statistics(const SearchResult& result) {
    // Include aborted-iteration work in the totals used for experiments.
    send("info nodes " + std::to_string(result.nodes) + " time " + std::to_string(result.elapsed_ms));
    std::ostringstream out;
    out << "info string stats";
    for (const auto& option : boolean_options)
        out << ' ' << option.name << '=' << result.statistics.*option.events;
    out << " NullProbes=" << result.statistics.null_probes;
    send(out.str());
}

}

struct UciSession::Impl {
    Engine engine;
    board::board_state position;
    std::atomic_bool stop{false};
#ifndef __EMSCRIPTEN__
    std::thread worker;
    std::condition_variable finished;
    std::mutex finished_mutex;
#endif
    Impl() {
        piece_attacks::init_all();
        position = board_utils::parse_fen(start_fen);
        engine.set_history({board_utils::repetition_key(position)});
#ifdef __EMSCRIPTEN__
        // Asyncify yields to the worker event loop so stop/isready can arrive.
        engine.set_event_pump([] { emscripten_sleep(0); });
#endif
    }
    ~Impl() { stop_search(); }
    void stop_search() {
#ifdef __EMSCRIPTEN__
        stop.store(true);
#else
        { std::lock_guard lock(finished_mutex); stop.store(true); }
        finished.notify_all();
        if (worker.joinable()) worker.join();
#endif
    }
    bool command(const std::string& line) {
        std::istringstream input(line);
        std::string command;
        input >> command;
        try {
            if (command == "uci") {
                send("id name Cpp-Chess\nid author Cpp-Chess contributors");
                for (const auto& option : boolean_options)
                    send("option name " + std::string(option.name) + " type check default " + (engine.settings().*option.member ? "true" : "false"));
                send("option name Hash type spin default " + std::to_string(engine.settings().hash_mb) + " min 1 max " + std::to_string(tuning::max_hash_mb));
                send("option name MoveOverhead type spin default " + std::to_string(engine.settings().move_overhead_ms) + " min 0 max 5000");
                send("option name Clear Hash type button\nuciok");
            } else if (command == "isready") send("readyok");
            else if (command == "stop") stop_search();
            else if (command == "quit") { stop_search(); return false; }
            else if (command == "ucinewgame") {
                stop_search(); engine.clear_hash(); engine.clear_repetition_table();
            } else if (command == "setoption") {
                stop_search();
                std::string token, name, value;
                if (!(input >> token) || token != "name") throw std::invalid_argument("setoption requires name");
                while (input >> token && token != "value") { if (!name.empty()) name += ' '; name += token; }
                if (token == "value") input >> value;
                auto settings = engine.settings();
                if (name == "Clear Hash") { engine.clear_hash(); return true; }
                bool found = false;
                for (const auto& option : boolean_options) if (name == option.name) {
                    if (value != "true" && value != "false") throw std::invalid_argument("check option requires true or false");
                    settings.*option.member = value == "true"; found = true;
                }
                if (name == "Hash") { settings.hash_mb = number(value, 1, tuning::max_hash_mb); found = true; }
                if (name == "MoveOverhead") { settings.move_overhead_ms = number(value, 0, 5000); found = true; }
                if (!found) throw std::invalid_argument("unknown option: " + name);
                engine.configure(settings);
            } else if (command == "position") {
                stop_search();
                std::string token, fen;
                input >> token;
                if (token == "startpos") { fen = start_fen; input >> token; }
                else if (token == "fen") {
                    for (int i = 0; i < 6; ++i) {
                        if (!(input >> token)) throw std::invalid_argument("position fen requires six fields");
                        if (i) fen += ' '; fen += token;
                    }
                    token.clear(); input >> token;
                } else throw std::invalid_argument("position requires startpos or fen");
                auto next = board_utils::parse_fen(fen);
                std::vector<U64> history{board_utils::repetition_key(next)};
                if (!token.empty() && token != "startpos" && token != "moves") throw std::invalid_argument("expected moves");
                if (token == "moves") while (input >> token) {
                    auto move = board::encode_move(next, token);
                    if (!move) throw std::invalid_argument("illegal move: " + token);
                    next = board::make_move(next, move);
                    history.push_back(board_utils::repetition_key(next));
                }
                position = next; engine.set_history(std::move(history));
            } else if (command == "go") {
                stop_search();
                SearchLimits limits;
                bool infinite = false;
                int wtime = -1, btime = -1, winc = 0, binc = 0, moves_to_go = tuning::default_moves_to_go;
                std::vector<std::string> tokens;
                std::string token;
                while (input >> token) tokens.push_back(token);
                for (size_t i = 0; i < tokens.size(); ++i) {
                    token = tokens[i];
                    if (token == "infinite") { infinite = true; continue; }
                    if (token == "ponder") throw std::invalid_argument("pondering is not supported");
                    if (token == "searchmoves") {
                        limits.restrict_root_moves = true;
                        while (i + 1 < tokens.size()) {
                            auto move = board::encode_move(position, tokens[i + 1]);
                            if (!move) break;
                            limits.root_moves.push_back(move); ++i;
                        }
                        if (limits.root_moves.empty()) throw std::invalid_argument("searchmoves requires legal moves");
                        continue;
                    }
                    if (i + 1 >= tokens.size()) throw std::invalid_argument("missing go value");
                    int value = number(tokens[++i]);
                    if (token == "depth") limits.depth = std::clamp(value, 1, tuning::max_ply - 1);
                    else if (token == "nodes") limits.nodes = std::max(1, value);
                    else if (token == "movetime") limits.movetime_ms = std::max(1, value);
                    else if (token == "mate") limits.depth = std::clamp(value, 1, (tuning::max_ply - 1) / 2) * 2;
                    else if (token == "wtime") wtime = value;
                    else if (token == "btime") btime = value;
                    else if (token == "winc") winc = value;
                    else if (token == "binc") binc = value;
                    else if (token == "movestogo") moves_to_go = std::max(1, value);
                    else throw std::invalid_argument("unknown go parameter: " + token);
                }
                int remaining = position.side == constants::white ? wtime : btime;
                int increment = position.side == constants::white ? winc : binc;
                if (!infinite && !limits.movetime_ms && remaining >= 0) {
                    int available = std::max(1, remaining - engine.settings().move_overhead_ms);
                    limits.movetime_ms = std::clamp(remaining / moves_to_go + increment / tuning::increment_divisor, 1, available);
                }
                if (infinite) limits.movetime_ms = 0;
                stop.store(false);
                auto search = [this, snapshot = position, limits, infinite] {
                    try {
                        auto result = engine.search(snapshot, limits, &stop, report);
                        if (infinite) {
#ifdef __EMSCRIPTEN__
                            while (!stop.load()) emscripten_sleep(5);
#else
                            std::unique_lock lock(finished_mutex);
                            finished.wait(lock, [&] { return stop.load(); });
#endif
                        }
                        report_statistics(result);
                        send("bestmove " + board::move_to_string(result.best_move));
                    } catch (const std::exception& error) {
                        send(std::string("info string search error: ") + error.what());
                        send("bestmove 0000");
                    }
                };
#ifdef __EMSCRIPTEN__
                search(); // worker adapter serializes state-changing commands
#else
                worker = std::thread(std::move(search));
#endif
            }
        } catch (const std::exception& error) { send(std::string("info string error: ") + error.what()); }
        return true;
    }
};

UciSession::UciSession() : impl(std::make_unique<Impl>()) {}
UciSession::~UciSession() = default;
bool UciSession::command(const std::string& line) { return impl->command(line); }

int run_uci() {
    UciSession session;
    std::string line;
    while (std::getline(std::cin, line) && session.command(line)) {}
    return 0;
}
