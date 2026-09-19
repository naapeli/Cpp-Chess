# Cpp-Chess

A C++20 chess engine with a native UCI executable and a WebAssembly browser interface.
[Play the published browser build](https://naapeli.github.io/Cpp-Chess/).

## Build and test

Use C++20, CMake, Python 3.11+ and uv. Compile explicitly; pytest never builds
C++ or changes `sys.path`/`PYTHONPATH`:

Python 3.11 is a minimum, not a pinned interpreter version; newer versions are
allowed. Dependencies also use minimum versions without upper bounds. The chess
library is installed directly as `chess`; `python-chess` is its old package name.
`uv.lock` records the versions used for a reproducible test run. To select the
newest compatible package releases and update the lockfile, run `uv sync --upgrade`.
Use `uv sync --python 3.14 --upgrade` to also select a newer interpreter (replace
`3.14` with your preferred version). Ordinary `uv sync --locked` keeps the tested
versions recorded in the lockfile.

```sh
uv sync --locked
uv run --frozen cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
uv run --frozen python -m pytest --engine build/engine.exe
```

Use `build/engine` on Linux/macOS, `build/Release/engine.exe` with Visual Studio.
For MinGW on Windows add `-G Ninja` when first configuring. `ctest --test-dir build
-C Release --output-on-failure` runs the same three pytest suites. Configure CMake
through `uv run` so it finds the correct Python; for an existing cache, set
`-DPython3_EXECUTABLE=/absolute/path/to/.venv/Scripts/python.exe` (Windows) or
`.venv/bin/python` (Unix). Rebuild explicitly after editing C++ or settings.

There are three Python test files:

| Suite | What it measures/checks |
| --- | --- |
| `tests/test_perft.py` | Six known perft counts, depth zero, seeded legal-move comparisons against python-chess |
| `tests/test_evaluation.py` | Static evaluator symmetry/material; static score error and greedy static-move CP loss against Stockfish |
| `tests/test_engine.py` | Fixed-time search, best move, completed depth, nodes, score, PV, CP loss; UCI, interruption, search invariants and exact technique comparisons |

The C++ helper `engine_tests` supplies internal hash/board/search checks to pytest;
it is compiled by CMake with `BUILD_TESTING=ON`. It also runs under WebAssembly.
No `unittest` runner is used. Without Stockfish, evaluation quality is explicitly
skipped and engine search measurements omit quality fields; correctness tests
still run. Set `STOCKFISH` or pass `--stockfish PATH` for full measurements.

## Compare compiled versions

Download [Stockfish](https://github.com/official-stockfish/Stockfish/releases).
CI uses 17.1. Run the same pytest command against each executable:

```sh
uv run --frozen python -m pytest --engine build/engine.exe --stockfish /path/to/stockfish --movetime-ms 1000 --repeats 3 --label before --report reports/before.json
# Edit and explicitly rebuild, or point --engine at a different compiled binary:
uv run --frozen python -m pytest --engine build/engine.exe --stockfish /path/to/stockfish --movetime-ms 1000 --repeats 3 --label after --report reports/after.json --baseline reports/before.json
```

One runner, `python -m pytest`, runs all three suites. To measure just search use
`tests/test_engine.py::test_engine`; to measure just static evaluation use
`tests/test_evaluation.py::test_evaluation`. Reports are written only with
`--report`; a baseline cannot be overwritten. No stale reports are needed to run.

The predetermined positions live in `tests/positions.json`; override with
`--suite FILE`. The candidate receives **only `go movetime`**, default 200 ms per
position, with no requested depth or node limit. Iterative deepening goes as far
as the budget permits (subject to the engine's recursion limit). Scores/PVs/depth
come from the last completed iteration; nodes and time include the unfinished
iteration. A run that cannot finish depth one fails with a diagnostic. Fixed-time
benchmarks require iterative deepening. Each position starts with a cleared hash,
independently of gameplay's persistent hash.

JSON contains every position/repeat, best move, evaluation (`cp` or `mate`), PV,
completed depth, total nodes, engine time, wall time, technique counters, Stockfish
scores and CP loss. It also records binary/suite SHA-256 hashes, compiled option
defaults, overrides, host information, budgets and summaries. Defaults are 16 MiB
hash and 100,000 reference nodes; use `--hash-mb` and `--reference-nodes` to change
them. Keep hardware/build mode and budgets comparable. Lower CP loss is better;
more nodes or a greater nominal depth alone does not establish stronger play.

Stockfish uses one thread and a fresh hash to search the root freely, then the
same root restricted to the chosen move with the same node budget. CP loss is
the difference between these two **Stockfish** scores, from the root side's
perspective. Negative raw differences are preserved as reference search noise
and clamped only for aggregate CP loss. Mates stay separate from CP values.

The evaluation suite calls `evaluation::evaluate` directly through the native
`--evaluate` diagnostic, without negamax or quiescence. It records root static
score error versus Stockfish's searched score. For a decision-based CP loss, it
statically evaluates each immediate legal successor and selects the highest
negated value, with legal terminal outcomes handled first and UCI move order
breaking ties. Stockfish then scores that greedy move. This measures the quality
of the evaluator's one-ply choices; static CP error is a separate metric and
Stockfish's deeper searched score is not an exact static-evaluation oracle.

An optional `--baseline FILE` fails for new mate regressions, mean CP-loss/static
error increases over 10 cp, or individual increases over 100 cp. Adjust
`--max-mean-regression` and `--max-position-regression`. Baselines must match the
suite, reference binary, budgets, repeats and hash. Different candidate binaries
and options are intentional. Comparisons use matching positions/repeats where
scores are numeric; mate changes are checked separately. Timing varies with
machine load, so repeat promising results on an idle machine. These twelve
positions are a starter regression suite, not an Elo estimate.

## Compare pruning techniques

Use the same benchmark with a UCI override; no extra experiment script is needed:

```sh
uv run --frozen python -m pytest tests/test_engine.py::test_engine --engine build/engine.exe --stockfish /path/to/stockfish --engine-option DeltaPruning=true --report reports/delta-on.json
uv run --frozen python -m pytest tests/test_engine.py::test_engine --engine build/engine.exe --stockfish /path/to/stockfish --engine-option DeltaPruning=false --report reports/delta-off.json --baseline reports/delta-on.json
```

Repeat `--engine-option NAME=VALUE` for interactions. A positive CP-loss change
means the second configuration chose worse moves. Compare time/depth/nodes and
`technique_events` as well: zero events means the technique was not exercised.
The engine suite separately checks exact optimizations on/off over an identical
shallow horizon. Null-move pruning, LMR and delta pruning are selective, so score
differences are not automatically bugs; quiescence and extensions change the
horizon. Every switch is checked by the C++ invariants; expand positions and time
budgets to exercise deeper interactions.

## Automation

GitHub Actions has two jobs: native Release build plus pytest, and a Release
WebAssembly build plus UCI/internal checks and a Chromium frontend test. On supported base revisions,
the native job builds the previous revision and compares both binaries using the
**current** positions, harness and Stockfish binary. The initial transition records
a report without a quality baseline. Quality checks use repeated fixed-time
searches and configurable tolerances; they are regression alarms, not proof of
strength. Reports are uploaded for inspection even on failure.

The WebAssembly job uploads a ready-to-serve `website` artifact containing
`index.html` and all `website/` assets, including freshly compiled JS/Wasm, from the tested
source. It does not commit generated binaries or change the repository's current
GitHub Pages publishing configuration. The existing published assets remain in
place until you publish the artifact or configure Pages to deploy it.

## Settings and modules

**`include/Engine/settings.h` is the single source for engine tuning defaults**:
pruning/search switches, depth and extension limits, margins, ordering bonuses,
time-management defaults, material values, game-phase weights, and piece-square
tables. Rebuild after changing defaults; both native and browser builds use them.
Fixed chess rules and board encodings live in `utils.h`.

UCI exposes every implemented search optimization separately:

| Area | UCI options |
| --- | --- |
| Bounds and selective pruning | `AlphaBetaPruning`, `NullMovePruning`, `LateMoveReductions` |
| Quiescence | `Quiescence`, `DeltaPruning`, `StandPatPruning` |
| Search windows and iterations | `PrincipalVariationSearch`, `AspirationWindows`, `IterativeDeepening` |
| Transpositions | `TranspositionTable`, `TTCutoffs`, `TTMoveOrdering` |
| Extensions | `CheckExtensions`, `PawnExtensions`, `ForcedMoveExtensions` |
| Ordering | `CaptureOrdering`, `PromotionOrdering`, `KillerOrdering`, `HistoryOrdering` |

For example, `setoption name DeltaPruning value false` disables delta pruning
inside quiescence. `Hash`, `MoveOverhead`, and `Clear Hash` are also available.
`AlphaBetaPruning=false` runs exhaustive negamax over the selected horizon and
bypasses dependent narrow-window/selective pruning (PVS, aspiration, null move,
LMR, delta, stand-pat cutoffs and TT cutoffs). TT ordering can still be used.
`TranspositionTable=false` disables both its cutoff and ordering users;
aspiration windows need iterative deepening; delta/stand-pat pruning need
quiescence. `IterativeDeepening=false` searches the requested depth once; use it
with `go depth` for meaningful measurements, since it may have no completed
iteration under a short time/node budget.

Stand pat is the static baseline defining quiescence; `StandPatPruning` controls
its early beta cutoff. Disabling `Quiescence` still resolves checks and recognizes
terminal positions. Legal moves, draw rules, mate scores, bounds on recursion and
stop handling are correctness/safety rules, not optional heuristics.

`SearchPath::NullMoveProbe` replaces the old `artificial` boolean. Null-move
pruning asks what happens if the current side passes. That pass is not legal
history: the probe and all descendants skip repetition/fifty-move claims and TT
reads/writes, and do not start another null probe. Quiescence receives the same
context because a probe can reach the horizon. Legal game branches use
`SearchPath::Game`. No fictitious position can thereby introduce a draw or cached
score into the legal search; this conservative policy favors correctness over
maximum null-subtree pruning.

| File | Responsibility |
| --- | --- |
| `src/Engine/engine.cpp` | Iterative deepening, budgets, repetition history, completed results |
| `src/Engine/search.cpp` | Negamax/alpha-beta, PVS, null moves, LMR, extensions |
| `src/Engine/quiescence.cpp` | Tactical continuation, check evasions, delta pruning |
| `src/Engine/move_ordering.cpp` | TT move, captures/promotions, killers and history |
| `src/Engine/evaluation.cpp` | Symmetric tapered evaluation and basic dead-material detection |
| `src/Engine/transpositionTable.cpp` | Zobrist keys and per-engine transposition storage |
| `src/Board/board.cpp` | FEN, legal text moves, board updates, clocks, hashing |
| `src/uci.cpp` | Protocol parsing, worker thread, synchronized output |
| `src/wasm_wrapper.cpp` | UCI line entry point for the browser worker |

The TT includes reversible-history context and the halfmove clock in its search
key to avoid reusing path-dependent draw scores incorrectly. It persists across searches/moves. `ucinewgame`, `Clear Hash`, browser
position reset (via `ucinewgame`), or changing search options clears it; changing the hash size
reallocates it. Benchmarks explicitly start cold, while a game reuses entries. Engine initialization must precede FEN parsing in direct C++ clients.

`engine.cpp` is the search coordinator: it manages time/node/stop budgets,
repetition history, iterative depths and the last complete result. Recursive
negamax lives in `search.cpp`, and tactical continuation in `quiescence.cpp`.
`on_iteration` is an optional synchronous callback, called once after each fully
completed depth. UCI passes its reporting function to emit `info depth ...`; the
browser can omit it. An interrupted depth never replaces the completed score/PV.

## UCI

Launch the native executable and communicate using standard input/output:

```text
uci
isready
setoption name NullMovePruning value false
position startpos moves e2e4 e7e5
go depth 5
```

The engine emits `info depth ... score cp|mate ... nodes ... time ... pv ...` and
one `bestmove` for a completed search. `bestmove 0000` means no legal move.
Supported commands include `uci`, `isready`, `ucinewgame`, `setoption`,
`position startpos|fen ... moves ...`, `go depth|nodes|movetime`, time controls
(`wtime`, `btime`, `winc`, `binc`, `movestogo`), `searchmoves`, `go infinite`,
`stop`, and `quit`. `go mate N` uses a depth limit of 2N plies. Search runs on a
worker thread, so `isready`, `stop`, and `quit` remain responsive. Pondering,
MultiPV, Chess960, and tablebases are not implemented or advertised.
Invalid positions/options produce `info string error` and preserve the previous
position. Timeouts return the last completed iteration or a legal fallback.
For test diagnostics, `engine --evaluate`, `engine --legal-moves`, and
`engine --perft DEPTH` each accept one FEN per stdin line and emit one result per
line. `--evaluate` returns side-to-move static centipawns. With no arguments the
executable speaks normal UCI.
See the [original UCI specification](https://www.shredderchess.com/chess-features/uci-universal-chess-interface.html).

## Browser build and local website

The frontend uses the same UCI parser as the native executable. The old direct
`get_best_move`/`make_move` browser API has been replaced by a UCI line interface.
A dedicated Web Worker owns the WebAssembly engine. [Asyncify](https://emscripten.org/docs/porting/asyncify.html) allows search to
yield so `stop` and `isready` remain responsive; no cross-origin isolation headers
or server-side engine are needed. All assets, including chess.js 1.4.0 and its
license, are local to `website/`.

Configure the Release build to write its generated files into the website, then
serve the repository with uv (compilation stays explicit):

```powershell
# Windows with the Emscripten SDK installed:
$env:EM_WORKAROUND_PYTHON_BUG_34780 = '1'
C:/emsdk/upstream/emscripten/emcmake.bat cmake -S . -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Release -DCHESS_WASM_OUTPUT_DIR="$PWD/website"
cmake --build build-wasm --parallel 4
ctest --test-dir build-wasm --output-on-failure
uv run --frozen python -m http.server 8000 --bind 127.0.0.1
```

Open **http://127.0.0.1:8000**. Stop the foreground server with Ctrl+C. On Unix use
`emcmake cmake` and omit the Windows environment workaround. Without
`CHESS_WASM_OUTPUT_DIR`, files go into the build directory; copy both `engine.js`
and `engine.wasm` to `website/` together. Reload the browser after rebuilding.

Click or drag pieces to move. Promotion offers queen, rook, bishop and knight.
Choose your side independently of flipping the board. `Engine move` plays one
move for the current side; `Start self-play` makes the engine play both sides until
you pause or the game ends. Pause/Stop accepts the last completed search result
and schedules no further moves. New game, Undo and loading a FEN cancel the old
search and discard its result. Loading a position pauses automatic play.

FEN accepts a **halfmove clock of 0**; the separate fullmove number must be at
least 1 (for example the final fields `0 1`). Invalid input leaves the current
position intact. Full move history is sent with `position ... moves ...`, so
repetition detection and transposition-table reuse work during games and self-play.
Analysis shows the last completed depth, total nodes, White's score, and PV.

The protocol smoke test checks the compiled module's UCI handshake, positions,
search, terminal positions and asynchronous stop/readiness. To test the actual
frontend in installed Chrome against your running local server:

```sh
uv sync --locked
uv run --frozen python -m pytest tests/test_engine.py::test_website --website-url http://127.0.0.1:8000 --browser-channel chrome
```

This optional browser check is part of `test_engine`; ordinary native runs skip
it unless `--website-url` is supplied. It covers human play, zero-halfmove FENs,
invalid input, self-play/pause, replacing positions during search, promotion,
terminal states, side switching and mobile overflow. Desktop/mobile screenshots
are saved under `build/` for review.

## Correctness coverage and fixes

Tests cover six standard perft positions, complete legal move sets from seeded
random games against python-chess, incremental hashes/occupancies, castling,
en passant, promotions, UCI notation, FEN rejection, evaluation color symmetry,
mates/stalemates at the horizon, fifty-move/threefold draws, TT mate-distance
normalization, deterministic search, feature switches, legal PVs and interruption.

The refactor fixes reversed Zobrist/killer/history array dimensions, uninitialized
search tables, negative/out-of-range ply indexing, stale null-move hashes,
unchecked TT move hints, unnormalized TT mate scores, incomplete timeout results,
stand-pat while checked, omitted quiet evasions, horizon stalemates, incorrect
color/boolean handling in evaluation, lost castling rights on rook captures,
en-passant text parsing, malformed UCI move strings, and hashing before browser
initialization. Repetition counts now include the root, require three occurrences,
and ignore en-passant rights when no legal en-passant capture exists. Search treats
a current threefold/fifty-move position as a draw; checkmate takes precedence.
Basic insufficient-material cases are recognized; this is not a full dead-position
solver or a claim that every possible engine bug is eliminated.
