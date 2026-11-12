"""Board tests: plug the Keepsake board into this computer and run

    python3 tools/board/run_board_tests.py            # the standard checks
    python3 tools/board/run_board_tests.py --flash    # build + flash this checkout first
    python3 tools/board/run_board_tests.py --interactive --destructive   # everything

Writes a report (report.md, results.json and screenshots) to build/board-tests/<time>/ and exits
non-zero if anything failed. See docs/board-tests.md.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from keepsake_board import Board, BoardError, black_fraction  # noqa: E402

REPO = Path(__file__).resolve().parents[2]


@dataclass
class Result:
    name: str
    status: str  # pass / warn / fail / skip
    detail: str = ""
    files: list = field(default_factory=list)


class Run:
    def __init__(self, board: Board, out: Path, options):
        self.board = board
        self.out = out
        self.options = options
        self.results: list[Result] = []

    def add(self, name, status, detail="", files=None):
        self.results.append(Result(name, status, detail, files or []))
        mark = {"pass": "✓", "warn": "!", "fail": "✗", "skip": "-"}[status]
        print(f"  {mark} {name}: {detail}")

    def shot(self, label: str) -> tuple:
        path = self.out / "screens" / f"{len(list((self.out / 'screens').glob('*.png'))) + 1:02d}-{label}.png"
        reply, pixels = self.board.screenshot(path)
        return reply, pixels, path


# --- the checks ---------------------------------------------------------------------------

def check_version(run: Run):
    reply = run.board.command("version")
    expected = (REPO / "firmware" / "version.txt").read_text().strip()
    detail = f"{reply['version']} on {reply['partition']}, up {reply['uptime_ms'] // 1000} s"
    if reply["version"] != expected:
        run.add("version", "warn", detail + f" (this checkout is {expected}: flash with --flash?)")
    else:
        run.add("version", "pass", detail)


def check_selftest(run: Run):
    reply = run.board.command("selftest", timeout=30.0)
    for check in reply["checks"]:
        run.add(f"selftest/{check['name']}", check["status"], check["detail"])


def check_screenshot(run: Run):
    reply, pixels, path = run.shot("now")
    ink = black_fraction(pixels)
    ok = 0.005 < ink < 0.9
    run.add("screenshot", "pass" if ok else "fail",
            f"{reply['screen']}, {ink:.1%} black ({'looks drawn' if ok else 'blank or solid?'})",
            [str(path)])


def check_card_round_trip(run: Run):
    payload = f"keepsake board test {time.time():.0f}\n".encode()
    put = run.board.put_file("/.keepsake/boardtest.txt", payload)
    back = run.board.get_file("/.keepsake/boardtest.txt") if put.ok else None
    run.board.command("rm", "/.keepsake/boardtest.txt")
    ok = back == payload
    run.add("sd/round-trip", "pass" if ok else "fail",
            "put/get/rm over the console" if ok else f"put={put.ok}, got {back!r}")


def go_home(run: Run, limit: int = 6) -> str:
    """Back out (quick Record/BOOT taps) until the screen stops changing."""
    screen = run.board.command("screen")["screen"]
    for _ in range(limit):
        if screen == "home":
            break
        time.sleep(0.6)
        screen = run.board.command("press", "action", "click")["screen"]
        time.sleep(0.8)
        screen = run.board.command("screen")["screen"]
    return screen


def check_ui_tour(run: Run):
    """Moves around with simulated button presses and screenshots each stop. Checks that
    presses do something and that back gets home again."""
    start = run.board.command("screen")["screen"]
    if start in ("onboarding", "lock_screen"):
        run.add("ui/tour", "skip", f"board is on {start}; finish onboarding / unlock first")
        return
    home = go_home(run)
    if home != "home":
        run.add("ui/back-to-home", "fail", f"stuck on {home} after pressing back")
        return
    _, _, home_path = run.shot("home")
    files = [str(home_path)]
    visited = []
    for step in range(4):
        run.board.command("press", "down", "click")
        time.sleep(0.6)
        run.board.command("press", "fn", "click")
        time.sleep(1.5)
        screen = run.board.command("screen")["screen"]
        visited.append(screen)
        if screen != "home":
            _, _, path = run.shot(screen)
            files.append(str(path))
        if go_home(run) != "home":
            run.add("ui/tour", "fail", f"couldn't get back home from {screen}", files)
            return
    opened = [s for s in visited if s != "home"]
    run.add("ui/tour", "pass" if opened else "warn",
            ("opened " + ", ".join(opened) + " and came back") if opened else
            "presses didn't open anything from home (check the screenshots)", files)


def check_buttons_by_hand(run: Run):
    """Asks the person at the computer to press each real button, and looks for it in the log."""
    labels = {"ACTION": "the Record / BOOT button (quick tap)", "UP": "the rocker UP",
              "DOWN": "the rocker DOWN", "FN": "the rocker press-in (FN)"}
    for label, description in labels.items():
        start = len(run.board.log)
        print(f"\n  >>> Press {description} now (10 s)...", flush=True)
        line = run.board.wait_for_log(lambda l, lab=label: f"{lab} event=PRESS_DOWN" in l, 10.0, start)
        run.add(f"buttons/{label.lower()}", "pass" if line else "fail",
                "seen" if line else "no press seen in the log")
        go_home(run)


def check_crash_report(run: Run):
    before = run.board.command("ls", "/.keepsake/crash")
    names_before = {e["name"] for e in before.get("entries", [])} if before.ok else set()
    run.board.command("crash", "yes")
    run.board.reconnect()
    run.board.wait_ready()
    after = run.board.command("ls", "/.keepsake/crash")
    new = sorted(e["name"] for e in after.get("entries", []) if e["name"] not in names_before) if after.ok else []
    reports = [n for n in new if n.endswith(".txt")]
    if not reports:
        run.add("crash-report", "fail", "no new report in /.keepsake/crash after a forced crash")
        return
    text = (run.board.get_file(f"/.keepsake/crash/{reports[-1]}") or b"").decode(errors="replace")
    path = run.out / reports[-1]
    path.write_text(text)
    ok = "Reset reason: panic" in text
    has_dump = any(n.endswith(".elf") for n in new)
    run.add("crash-report", "pass" if ok else "fail",
            f"{reports[-1]}{' + core dump' if has_dump else ' (no core dump)'}", [str(path)])


def check_provisioning_refusal(run: Run):
    """A broken provisioning file must import nothing and explain itself."""
    bad = b'{ "wifi": [ { "ssid": "BoardTest", "password": "short" } ] }'
    run.board.command("rm", "/.keepsake/provision-problems.txt")
    run.board.put_file("/.keepsake/provision.json", bad)
    run.board.command("reboot")
    run.board.reconnect()
    run.board.wait_ready()
    problems = run.board.get_file("/.keepsake/provision-problems.txt")
    still_there = run.board.get_file("/.keepsake/provision.json") is not None
    run.board.command("rm", "/.keepsake/provision.json")
    run.board.command("rm", "/.keepsake/provision-problems.txt")
    ok = problems is not None and b"wifi[0].password" in problems and still_there
    run.add("provisioning/refusal", "pass" if ok else "fail",
            "bad file kept, problems explained" if ok else f"problems file {'missing' if problems is None else 'wrong'}")


# --- main -----------------------------------------------------------------------------------

def flash(port: str | None) -> None:
    command = ["idf.py", "-C", str(REPO / "firmware")]
    if port:
        command += ["-p", port]
    command += ["build", "flash"]
    print("$ " + " ".join(command), flush=True)
    subprocess.run(command, check=True)


def write_report(run: Run) -> None:
    counts = {s: sum(1 for r in run.results if r.status == s) for s in ("pass", "warn", "fail", "skip")}
    lines = [f"# Keepsake board test {time.strftime('%Y-%m-%d %H:%M')}", "",
             f"**{counts['pass']} passed, {counts['warn']} warnings, {counts['fail']} failed, {counts['skip']} skipped** "
             f"on {run.board.port_name}", "", "| Check | Result | Detail |", "|---|---|---|"]
    for r in run.results:
        files = " ".join(f"[{Path(f).name}]({Path(f).relative_to(run.out)})" for f in r.files)
        lines.append(f"| {r.name} | {r.status} | {r.detail} {files} |")
    (run.out / "report.md").write_text("\n".join(lines) + "\n")
    (run.out / "results.json").write_text(json.dumps([r.__dict__ for r in run.results], indent=2))
    (run.out / "serial.log").write_text("\n".join(run.board.log) + "\n")


def main(argv: list) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", help="serial port (default: the first Espressif USB port)")
    parser.add_argument("--flash", action="store_true", help="build and flash this checkout first (needs idf.py)")
    parser.add_argument("--interactive", action="store_true", help="also ask you to press each real button")
    parser.add_argument("--destructive", action="store_true",
                        help="also crash the board on purpose and try a broken provisioning file (reboots it)")
    parser.add_argument("--out", type=Path, help="where to write the report (default build/board-tests/<time>)")
    options = parser.parse_args(argv)

    if options.flash:
        flash(options.port)
    out = options.out or REPO / "build" / "board-tests" / time.strftime("%Y%m%d-%H%M%S")
    (out / "screens").mkdir(parents=True, exist_ok=True)

    board = Board(options.port).open()
    print(f"Board on {board.port_name}; report in {out}")
    run = Run(board, out, options)
    if options.flash:
        board.wait_ready()
    checks = [check_version, check_selftest, check_screenshot, check_card_round_trip, check_ui_tour]
    if options.interactive:
        checks.append(check_buttons_by_hand)
    if options.destructive:
        checks += [check_crash_report, check_provisioning_refusal]
    try:
        for check in checks:
            print(f"\n{check.__name__.removeprefix('check_').replace('_', ' ')}:")
            try:
                check(run)
            except BoardError as error:
                run.add(check.__name__.removeprefix("check_"), "fail", str(error))
    finally:
        write_report(run)
        board.close()
    failed = [r for r in run.results if r.status == "fail"]
    print(f"\n{len(run.results) - len(failed)}/{len(run.results)} ok. Report: {out / 'report.md'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
