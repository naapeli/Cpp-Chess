"""Fixed-time engine measurements, search invariants and UCI behavior."""
from pathlib import Path
import subprocess

import chess
import chess.engine
import pytest

from tests.test_perft import differential_positions
from tests.uci_checks import check_uci
from tools.benchmark import compare_baseline, executable

EXACT = ("AlphaBetaPruning", "StandPatPruning", "AspirationWindows", "PrincipalVariationSearch",
         "TranspositionTable", "TTCutoffs", "TTMoveOrdering", "CaptureOrdering",
         "PromotionOrdering", "KillerOrdering", "HistoryOrdering", "IterativeDeepening")


@pytest.mark.parametrize("technique", EXACT)
def test_exact_search_techniques(engine_path, technique):
    # These optimizations must preserve the score over an identical horizon.
    # Selective pruning and extensions intentionally do not promise that.
    with chess.engine.SimpleEngine.popen_uci(engine_path) as engine:
        profile = {name: True for name in EXACT}
        profile.update(dict.fromkeys(("NullMovePruning", "LateMoveReductions", "DeltaPruning",
                                     "CheckExtensions", "PawnExtensions", "ForcedMoveExtensions"), False))
        profile["Quiescence"] = True
        for fen in (chess.STARTING_FEN, "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"):
            board = chess.Board(fen)
            scores = []
            for enabled in (True, False):
                engine.configure({**profile, technique: enabled, "Clear Hash": None})
                info = engine.analyse(board, chess.engine.Limit(depth=2, time=5), game=object())
                assert info.get("depth") == 2, f"{technique}: incomplete correctness search"
                scores.append(info["score"].pov(board.turn))
                if not enabled:
                    assert f"{technique}=0" in info.get("string", "").split()
            assert scores[0] == scores[1], f"{technique}: {fen}: {scores}"


def test_engine(benchmark, quality_gate):
    for repeat in range(benchmark.repeats):
        for item in benchmark.positions:
            benchmark.engine_row(item, repeat)
    quality_gate(benchmark, "engine")


def test_core_invariants(engine_path, pytestconfig):
    supplied = pytestconfig.getoption("core_tests")
    helper = Path(supplied) if supplied else Path(engine_path).with_name("engine_tests" + Path(engine_path).suffix)
    if not helper.is_file() and not supplied:
        pytest.skip("Build with BUILD_TESTING=ON for internal C++ invariant checks")
    result = subprocess.run([executable(helper)], text=True, capture_output=True, timeout=120)
    assert result.returncode == 0, result.stdout + result.stderr


def test_uci(engine_path):
    check_uci(engine_path)


def test_website(pytestconfig):
    url = pytestconfig.getoption("website_url")
    if not url:
        pytest.skip("Pass --website-url to check the frontend against a running local server")
    pytest.importorskip("playwright.sync_api")
    from tests.browser_checks import check_website
    check_website(url, pytestconfig.getoption("browser_channel"))


def test_sampled_pvs(engine_path):
    with chess.engine.SimpleEngine.popen_uci(engine_path) as engine:
        for board in differential_positions()[::7]:
            info = engine.analyse(board, chess.engine.Limit(depth=2), game=object())
            cursor = board.copy()
            for move in info.get("pv", []):
                assert move in cursor.legal_moves, (board.fen(), info)
                cursor.push(move)
            if board.is_checkmate():
                assert info["score"].pov(board.turn).mate() == 0
            elif board.is_stalemate():
                assert info["score"].pov(board.turn).score() == 0
            else:
                assert info.get("pv"), (board.fen(), info)


def test_baseline_gate():
    row = {"id": "a", "repeat": 0, "cp_loss": 20, "mate_regression": False}
    old = {"schema": 2, "method": {"budget": 1}, "engine": [row]}
    new = {**old, "engine": [{**row, "cp_loss": 50}]}
    assert compare_baseline(new, old, "engine", 10, 100)
    assert not compare_baseline(new, old, "engine", 40, 100)
    with pytest.raises(ValueError, match="Incompatible"):
        compare_baseline({**new, "method": {}}, old, "engine", 10, 100)
    with pytest.raises(ValueError, match="samples"):
        compare_baseline({**new, "engine": []}, old, "engine", 10, 100)
    mate = {**old, "engine": [{**row, "cp_loss": None, "mate_regression": True}]}
    assert compare_baseline(mate, old, "engine", 10, 100)
