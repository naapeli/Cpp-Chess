"""One pytest entry point, three suites, precompiled executables only."""
import json
import os
from pathlib import Path
import shutil

import pytest

from tools.benchmark import Benchmark, compare_baseline, executable, summary

ROOT = Path(__file__).resolve().parent


def pytest_addoption(parser):
    group = parser.getgroup("chess")
    group.addoption("--engine", default=str(ROOT / "build" / ("engine.exe" if os.name == "nt" else "engine")))
    group.addoption("--core-tests", help="Optional compiled C++ invariant checks (defaults beside engine)")
    group.addoption("--stockfish", default=os.environ.get("STOCKFISH") or shutil.which("stockfish"))
    group.addoption("--suite", default=str(ROOT / "tests/positions.json"))
    group.addoption("--movetime-ms", type=int, default=200)
    group.addoption("--reference-nodes", type=int, default=100000)
    group.addoption("--hash-mb", type=int, default=16)
    group.addoption("--repeats", type=int, default=1)
    group.addoption("--engine-option", action="append", default=[], metavar="NAME=VALUE")
    group.addoption("--label", default="", help="Build/version description stored in the report")
    group.addoption("--website-url", help="Run browser integration checks against this local website URL")
    group.addoption("--browser-channel", default="chrome", help="Installed Playwright browser channel")
    group.addoption("--report", help="Write JSON measurements here (nothing is written by default)")
    group.addoption("--baseline", help="Compare quality against a report from another compiled version")
    group.addoption("--max-mean-regression", type=float, default=10)
    group.addoption("--max-position-regression", type=float, default=100)


def pytest_configure(config):
    for name in ("movetime_ms", "reference_nodes", "hash_mb", "repeats"):
        if config.getoption(name) <= 0:
            raise pytest.UsageError(f"--{name.replace('_', '-')} must be positive")
    for name in ("max_mean_regression", "max_position_regression"):
        if config.getoption(name) < 0:
            raise pytest.UsageError(f"--{name.replace('_', '-')} must be nonnegative")
    baseline, output = config.getoption("baseline"), config.getoption("report")
    if baseline and not config.getoption("stockfish"):
        raise pytest.UsageError("--baseline requires --stockfish; quality must be measured")
    if baseline and output and Path(baseline).resolve() == Path(output).resolve():
        raise pytest.UsageError("--report must not overwrite --baseline")


@pytest.fixture(scope="session")
def engine_path(pytestconfig):
    return executable(pytestconfig.getoption("engine"))


@pytest.fixture(scope="session")
def benchmark(pytestconfig, engine_path):
    options = {}
    for setting in pytestconfig.getoption("engine_option"):
        if "=" not in setting:
            raise pytest.UsageError("--engine-option requires NAME=VALUE")
        name, value = setting.split("=", 1)
        if name in ("Hash", "Clear Hash"):
            raise pytest.UsageError("Use --hash-mb; hash is cleared between benchmark positions")
        options[name] = {"true": True, "false": False}.get(value.lower(), value)
    if options.get("IterativeDeepening") is False:
        raise pytest.UsageError("Fixed-time benchmarks require IterativeDeepening=true")
    reference = pytestconfig.getoption("stockfish")
    runner = Benchmark(engine_path, executable(reference) if reference else None,
                       pytestconfig.getoption("suite"), pytestconfig.getoption("movetime_ms"),
                       pytestconfig.getoption("reference_nodes"), pytestconfig.getoption("hash_mb"),
                       pytestconfig.getoption("repeats"), options, pytestconfig.getoption("label"))
    pytestconfig.chess_report = runner.report
    try:
        yield runner
    finally:
        runner.close(pytestconfig.getoption("report"))


@pytest.fixture
def quality_gate(pytestconfig):
    def check(benchmark, section):
        baseline = pytestconfig.getoption("baseline")
        if baseline:
            failures = compare_baseline(benchmark.report, json.loads(Path(baseline).read_text()), section,
                                        pytestconfig.getoption("max_mean_regression"),
                                        pytestconfig.getoption("max_position_regression"))
            benchmark.report.setdefault("failures", []).extend(failures)
            assert not failures, "\n".join(failures)
    return check


def pytest_terminal_summary(terminalreporter, config):
    report = getattr(config, "chess_report", None)
    if not report:
        return
    for section in ("evaluation", "engine"):
        if report[section]:
            values = summary(report[section])
            text = f"{section}: {values['samples']} samples"
            if values["mean_cp_loss"] is not None:
                text += f", mean CP loss={values['mean_cp_loss']:.2f} ({values['cp_samples']} numeric scores)"
                text += f", mate regressions={values['mate_regressions']}"
            if values["mean_absolute_error_cp"] is not None:
                text += f", static MAE={values['mean_absolute_error_cp']:.2f} cp"
            terminalreporter.write_line(text)
    if not report["method"]["reference_sha256"]:
        terminalreporter.write_line("Stockfish not supplied: search measurements have no CP-loss assessment.")
    if config.getoption("report"):
        terminalreporter.write_line(f"Measurements: {config.getoption('report')}")
