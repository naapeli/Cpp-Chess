"""Static evaluator only: invariants, score error and greedy-move CP loss."""
import chess
import chess.engine
import pytest

from tools.benchmark import measure, probe


def test_static_invariants(engine_path):
    advantage = chess.Board("7k/8/8/8/8/3Q4/8/K7 w - - 0 1")
    opposite = advantage.copy()
    opposite.turn = not opposite.turn
    boards = [chess.Board(), advantage, opposite, advantage.mirror(),
              chess.Board("7k/8/8/8/8/8/8/KB6 w - - 0 1")]
    start, good, bad, mirrored, drawn = map(int, probe(engine_path, "--evaluate", boards))
    assert start == drawn == 0
    assert good > 500
    assert good == -bad == mirrored


def test_evaluation(benchmark, quality_gate):
    if not benchmark.reference_path:
        pytest.skip("Pass --stockfish PATH (or set STOCKFISH) to measure evaluation quality")
    for repeat in range(benchmark.repeats):
        for item in benchmark.positions:
            benchmark.evaluation_row(item, repeat)
    quality_gate(benchmark, "evaluation")


def test_cp_metrics():
    assert measure(chess.engine.Cp(60), chess.engine.Cp(-40))["cp_loss"] == 100
    noisy = measure(chess.engine.Cp(20), chess.engine.Cp(40))
    assert noisy["cp_loss"] == 0 and noisy["raw_cp_loss"] == -20
    missed_mate = measure(chess.engine.Mate(3), chess.engine.Cp(900))
    assert missed_mate["cp_loss"] is None and missed_mate["mate_regression"]
    assert measure(chess.engine.Cp(-500), chess.engine.Mate(-2))["mate_regression"]
    assert not measure(chess.engine.Mate(-2), chess.engine.Mate(-5))["mate_regression"]
