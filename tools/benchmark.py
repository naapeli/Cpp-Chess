"""Shared measurements for pytest. Never builds C++ or changes import paths."""
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import platform
import shutil
import statistics
import subprocess
import time

import chess
import chess.engine


def executable(value):
    path = Path(shutil.which(str(value)) or value).resolve()
    if not path.is_file():
        raise ValueError(f"Executable not found: {value}. Compile it explicitly with CMake first.")
    return str(path)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def probe(engine, mode, boards, *args):
    result = subprocess.run([engine, mode, *map(str, args)],
                            input="\n".join(b.fen(en_passant="fen") for b in boards) + "\n",
                            text=True, capture_output=True, timeout=60, check=True)
    lines = result.stdout.splitlines()
    if len(lines) != len(boards):
        raise ValueError(f"Expected {len(boards)} diagnostic results, got {result.stdout!r}")
    return lines


def score_json(score):
    return {"mate": score.mate()} if score.is_mate() else {"cp": score.score()}


def measure(best, chosen):
    if best.is_mate() or chosen.is_mate():
        return {"cp_loss": None, "raw_cp_loss": None, "mate_regression": chosen < best}
    raw = best.score() - chosen.score()
    return {"cp_loss": max(0, raw), "raw_cp_loss": raw, "mate_regression": False}


def summary(rows):
    losses = [r["cp_loss"] for r in rows if r.get("cp_loss") is not None]
    errors = [r["absolute_error_cp"] for r in rows if r.get("absolute_error_cp") is not None]
    return {"samples": len(rows),
            "cp_samples": len(losses),
            "mean_cp_loss": statistics.mean(losses) if losses else None,
            "median_cp_loss": statistics.median(losses) if losses else None,
            "max_cp_loss": max(losses) if losses else None,
            "mean_absolute_error_cp": statistics.mean(errors) if errors else None,
            "mate_regressions": sum(r.get("mate_regression", False) for r in rows),
            "reference_inconsistencies": sum(r.get("raw_cp_loss") is not None and r["raw_cp_loss"] < 0 for r in rows),
            "total_nodes": sum(r.get("nodes", 0) for r in rows)}


def compare_baseline(report, baseline, section, mean_tolerance, position_tolerance):
    if report["schema"] != baseline.get("schema") or report["method"] != baseline.get("method"):
        raise ValueError("Incompatible baseline: suite, reference binary, budgets, repeats and hash must match")
    rows, previous = report[section], baseline.get(section, [])
    key = lambda r: (r["id"], r["repeat"])
    old = {key(r): r for r in previous}
    if not rows or len(old) != len(previous) or set(old) != {key(r) for r in rows}:
        raise ValueError(f"Baseline {section} samples differ or are missing")
    errors = []
    for metric in ("cp_loss", "absolute_error_cp"):
        deltas = []
        for row in rows:
            before, after = old[key(row)].get(metric), row.get(metric)
            if before is None or after is None:
                continue
            delta = after - before
            deltas.append(delta)
            if delta > position_tolerance:
                errors.append(f"{row['id']} repeat {row['repeat']}: {metric} increased by {delta:.1f}")
        if deltas and statistics.mean(deltas) > mean_tolerance:
            errors.append(f"{section}: mean {metric} increased by {statistics.mean(deltas):.1f}")
    for row in rows:
        if row.get("mate_regression") and not old[key(row)].get("mate_regression"):
            errors.append(f"{row['id']}: new mate regression")
    return errors


class Benchmark:
    def __init__(self, engine, stockfish, suite, movetime_ms, reference_nodes, hash_mb, repeats, options, label):
        self.engine_path, self.reference_path = engine, stockfish
        self.movetime_ms, self.reference_nodes = movetime_ms, reference_nodes
        self.hash_mb, self.repeats, self.options = hash_mb, repeats, options
        self.positions = json.loads(Path(suite).read_text())
        if not self.positions or len({p["id"] for p in self.positions}) != len(self.positions):
            raise ValueError("Position suite must be nonempty with unique IDs")
        for item in self.positions:
            board = chess.Board(item["fen"])
            if not board.is_valid() or board.is_game_over():
                raise ValueError(f"{item['id']}: expected a valid, nonterminal position")
        self.stack = ExitStack()
        self._candidate = self._reference = None
        self.reference_cache = {}
        self.report = {"schema": 2, "method": {
            "suite_sha256": digest(suite), "movetime_ms": movetime_ms,
            "reference_nodes": reference_nodes, "hash_mb": hash_mb, "repeats": repeats,
            "reference_sha256": digest(stockfish) if stockfish else None,
            "reference_threads": 1, "scoring": "same-root-forced-move-v2",
            "evaluation_policy": "greedy-static-successors-terminal-rules-uci-tiebreak"},
            "candidate": {"path": engine, "sha256": digest(engine), "label": label,
                          "options": options},
            "host": {"platform": platform.platform(), "processor": platform.processor()},
            "engine": [], "evaluation": []}

    @property
    def candidate(self):
        if self._candidate is None:
            self._candidate = self.stack.enter_context(chess.engine.SimpleEngine.popen_uci(self.engine_path))
            self._candidate.configure({"Hash": self.hash_mb, **self.options})
            self.report["candidate"]["id"] = self._candidate.id
            self.report["candidate"]["compiled_defaults"] = {
                name: opt.default for name, opt in self._candidate.options.items() if opt.type != "button"}
        return self._candidate

    @property
    def reference(self):
        if self._reference is None:
            if not self.reference_path:
                raise ValueError("Stockfish is required for quality measurements")
            self._reference = self.stack.enter_context(chess.engine.SimpleEngine.popen_uci(self.reference_path))
            if "Stockfish" not in self._reference.id.get("name", ""):
                raise ValueError("--stockfish must identify itself as Stockfish")
            self._reference.configure({"Threads": 1, "Hash": self.hash_mb})
            self.report["reference_id"] = self._reference.id
        return self._reference

    def reference_info(self, board, move=None):
        key = (board.fen(), move.uci() if move else None)
        if key not in self.reference_cache:
            self.reference.configure({"Clear Hash": None})
            self.reference_cache[key] = self.reference.analyse(
                board, chess.engine.Limit(nodes=self.reference_nodes), game=object(),
                root_moves=[move] if move else None)
        return self.reference_cache[key]

    def quality(self, board, move):
        best = self.reference_info(board)
        reference_move = best["pv"][0]
        chosen = best if move == reference_move else self.reference_info(board, move)
        best_score, chosen_score = best["score"].pov(board.turn), chosen["score"].pov(board.turn)
        return {"reference_move": reference_move.uci(), "agreement": move == reference_move,
                "reference_score": score_json(best_score), "chosen_reference_score": score_json(chosen_score),
                "reference_depth": best["depth"], "forced_reference_depth": chosen["depth"],
                **measure(best_score, chosen_score)}

    def engine_row(self, item, repeat):
        board = chess.Board(item["fen"])
        # Explicitly cold positions; gameplay itself retains its transposition table.
        self.candidate.configure({"Clear Hash": None})
        start = time.perf_counter()
        selected = self.candidate.play(board, chess.engine.Limit(time=self.movetime_ms / 1000),
                                       game=object(), info=chess.engine.INFO_ALL)
        elapsed = (time.perf_counter() - start) * 1000
        if selected.move not in board.legal_moves:
            raise ValueError(f"{item['id']}: illegal best move {selected.move}")
        info = selected.info
        pv = info.get("pv", [])
        cursor = board.copy()
        for move in pv:
            if move not in cursor.legal_moves:
                raise ValueError(f"{item['id']}: illegal PV {pv}")
            cursor.push(move)
        if info.get("depth", 0) < 1 or not pv or pv[0] != selected.move or "score" not in info:
            raise ValueError(f"{item['id']}: no completed iteration in {self.movetime_ms} ms: {info}")
        if info.get("nodes", 0) <= 0:
            raise ValueError(f"{item['id']}: missing node count")
        row = {**item, "repeat": repeat, "best_move": selected.move.uci(),
               "depth": info["depth"], "nodes": info["nodes"],
               "evaluation": score_json(info["score"].pov(board.turn)),
               "pv": [m.uci() for m in pv], "time_ms": info.get("time", 0) * 1000,
               "wall_time_ms": elapsed, "quality_available": bool(self.reference_path)}
        if info.get("string", "").startswith("stats "):
            row["technique_events"] = {k: int(v) for k, v in
                                       (s.split("=") for s in info["string"].split()[1:])}
        if self.reference_path:
            row.update(self.quality(board, selected.move))
        self.report["engine"].append(row)
        return row

    def evaluation_row(self, item, repeat):
        board = chess.Board(item["fen"])
        moves = sorted(board.legal_moves, key=lambda m: m.uci())
        children = []
        for move in moves:
            child = board.copy()
            child.push(move)
            children.append(child)
        values = list(map(int, probe(self.engine_path, "--evaluate", [board, *children])))
        # Terminal rules are not evaluator heuristics. No quiescence or search.
        ranks = [(1, 0) if child.is_checkmate() else
                 (0, 0) if child.is_game_over() else (0, -value)
                 for child, value in zip(children, values[1:])]
        index = max(range(len(moves)), key=lambda i: ranks[i])
        quality = self.quality(board, moves[index])
        reference_cp = quality["reference_score"].get("cp")
        error = values[0] - reference_cp if reference_cp is not None else None
        row = {**item, "repeat": repeat, "static_evaluation_cp": values[0],
               "greedy_move": moves[index].uci(), "signed_error_cp": error,
               "absolute_error_cp": abs(error) if error is not None else None, **quality}
        self.report["evaluation"].append(row)
        return row

    def close(self, output=None):
        self.stack.close()
        if output:
            self.report["summary"] = {s: summary(self.report[s]) for s in ("evaluation", "engine")}
            path = Path(output)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(json.dumps(self.report, indent=2) + "\n")
