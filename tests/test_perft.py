"""Move-generation counts and complete legal move sets."""
import random

import chess
import pytest

from tools.benchmark import probe

PERFT = [
    (chess.STARTING_FEN, 4, 197281),
    ("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862),
    ("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238),
    ("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467),
    ("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379),
    ("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 3, 89890),
]


@pytest.mark.parametrize("fen,depth,expected", PERFT)
def test_perft(engine_path, fen, depth, expected):
    assert int(probe(engine_path, "--perft", [chess.Board(fen)], depth)[0]) == expected


def test_perft_zero(engine_path):
    assert probe(engine_path, "--perft", [chess.Board()], 0) == ["1"]


def differential_positions():
    rng = random.Random(20260919)
    positions = [chess.Board(fen) for fen, _, _ in PERFT]
    positions += [chess.Board(fen) for fen in (
        "5b1k/8/8/3pP3/1K6/8/8/8 w - d6 0 1",
        "7k/5b2/8/3pP3/2K5/8/8/8 w - d6 0 1",
        "7k/8/8/r4pPK/8/8/8/8 w - f6 0 1",
        "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1",
        "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1",
    )]
    for _ in range(12):
        board = chess.Board()
        for ply in range(100):
            if board.is_game_over():
                break
            if ply % 3 == 0:
                positions.append(board.copy())
            board.push(rng.choice(list(board.legal_moves)))
    return positions


def test_legal_move_sets(engine_path):
    positions = differential_positions()
    for board, line in zip(positions, probe(engine_path, "--legal-moves", positions)):
        expected, actual = {m.uci() for m in board.legal_moves}, line.split()
        assert len(actual) == len(set(actual)), f"Duplicate moves: {board.fen()}"
        assert set(actual) == expected, f"{board.fen()}: missing={expected-set(actual)}, extra={set(actual)-expected}"
