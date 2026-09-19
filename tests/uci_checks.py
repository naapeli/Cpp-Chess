"""Dependency-free black-box UCI tests, including commands during search."""
import queue
import re
import subprocess
import threading
import time


def check_uci(engine_path):
    p = subprocess.Popen([engine_path], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, text=True, bufsize=1)
    lines = queue.Queue()
    threading.Thread(target=lambda: [lines.put(line.strip()) for line in p.stdout], daemon=True).start()

    def send(command):
        p.stdin.write(command + "\n")
        p.stdin.flush()

    def until(prefix, timeout=10):
        deadline = time.monotonic() + timeout
        received = []
        while time.monotonic() < deadline:
            try:
                line = lines.get(timeout=max(0.01, deadline - time.monotonic()))
            except queue.Empty:
                break
            received.append(line)
            assert line.startswith(("id ", "option ", "uciok", "readyok", "info ", "bestmove ")), line
            if line.startswith(prefix):
                return line, received
        raise AssertionError(f"Timed out waiting for {prefix}: {received}")

    def bestmove(command):
        send(command)
        line, received = until("bestmove ")
        assert re.fullmatch(r"bestmove (?:[a-h][1-8][a-h][1-8][qrbn]?|0000)", line), line
        return line.split()[1], received

    try:
        send("uci")
        _, handshake = until("uciok")
        options = [line.split("name ")[1].split(" type")[0] for line in handshake if "type check" in line]
        assert len(options) == 19 and "DeltaPruning" in options and "AlphaBetaPruning" in options, handshake
        send("isready"); until("readyok")
        for name in options:
            send(f"setoption name {name} value false")
        send("setoption name Hash value 1")
        send("setoption name Clear Hash")
        send("position startpos moves e2e4 e7e5")
        move, info = bestmove("go depth 2")
        assert move != "0000" and any("info depth 2 " in line for line in info), info
        send("position startpos")
        assert bestmove("go depth 2 searchmoves e2e4")[0] == "e2e4"
        send("position fen 7k/P7/8/8/8/8/8/7K w - - 0 1")
        assert bestmove("go depth 1 searchmoves a7a8n")[0] == "a7a8n"
        send("position fen 7k/6Q1/6K1/8/8/8/8/8 b - - 0 1")
        assert bestmove("go depth 1")[0] == "0000"
        send("position fen 7k/5Q2/6K1/8/8/8/8/8 b - - 0 1")
        assert bestmove("go depth 1")[0] == "0000"
        send("position startpos")
        send("position startpos moves e2e5")
        until("info string error:")
        assert bestmove("go depth 1 searchmoves e2e4")[0] == "e2e4", "invalid position must be atomic"
        send("position fen garbage")
        until("info string error:")
        send("setoption name Hash value garbage")
        until("info string error:")
        for command in ("go movetime 1", "go nodes 1", "go wtime 30 btime 30 winc 0 binc 0 movestogo 1"):
            started = time.monotonic()
            assert bestmove(command)[0] != "0000"
            assert time.monotonic() - started < 3
        send("go infinite")
        send("isready")
        _, received = until("readyok", 3)
        assert not any(line.startswith("bestmove") for line in received)
        send("stop")
        until("bestmove ", 3)
        # Even an infinite search that finishes immediately waits for stop.
        send("position fen 7k/6Q1/6K1/8/8/8/8/8 b - - 0 1")
        send("go infinite")
        time.sleep(0.05)
        send("isready")
        _, received = until("readyok", 3)
        assert not any(line.startswith("bestmove") for line in received)
        send("stop"); assert until("bestmove ")[0] == "bestmove 0000"
        send("ucinewgame")
        send("position startpos")
        send("go infinite")
        send("quit")
        p.wait(timeout=3)
        assert p.returncode == 0
        assert not p.stderr.read()
        print("UCI protocol checks passed")
    finally:
        if p.poll() is None:
            p.kill()
            p.wait()
