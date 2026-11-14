#!/usr/bin/env python3
"""ARMOR-NODE-SOLAR - guard on tools/frames_to_fixture.py: it keeps the entries of a capture, skips cut lines and refuses a sign-in.

Usage:  python tests/check_frames_tool.py
Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
"""
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import frames_to_fixture  # noqa: E402


def main() -> int:
    problems: list[str] = []
    request = "51504947530d".lower()
    lines = [
        f"FRAME p1 tx {request}\n",
        "FRAME p1 rx 283233322e30\n",
        "FRAME p2 rx 2832\n",
        "something else in the console log\n",
        "FRAME p1 rx 28323\n",                      # an odd number of digits: cut
        "FRAME p1 rx " + "login x".encode().hex() + "\n",   # a sign-in is never kept
        "FRAME p3 rx zz\n",                          # not hexadecimal
    ]
    good, skipped = frames_to_fixture.convert(lines)
    if good != [f"p1 tx {request}", "p1 rx 283233322e30", "p2 rx 2832"]:
        problems.append(f"unexpected entries: {good}")
    if skipped != 3:
        problems.append(f"expected 3 skipped lines, got {skipped}")
    with tempfile.TemporaryDirectory() as folder:
        source = Path(folder) / "capture.log"
        target = Path(folder) / "fixture.hex"
        source.write_text("".join(lines), encoding="utf-8")
        done = subprocess.run([sys.executable, str(ROOT / "tools" / "frames_to_fixture.py"), str(source), str(target)], capture_output=True, text=True, timeout=60, check=False)
        if done.returncode != 0 or target.read_text(encoding="utf-8").count("\n") != 3:
            problems.append("the tool did not write the three entries")
        empty = Path(folder) / "empty.log"
        empty.write_text("nothing here\n", encoding="utf-8")
        refused = subprocess.run([sys.executable, str(ROOT / "tools" / "frames_to_fixture.py"), str(empty), str(target)], capture_output=True, text=True, timeout=60, check=False)
        if refused.returncode != 1:
            problems.append("a file with no entry must exit 1")
    helped = subprocess.run([sys.executable, str(ROOT / "tools" / "frames_to_fixture.py"), "--help"], capture_output=True, text=True, timeout=60, check=False)
    if helped.returncode != 0:
        problems.append("--help must exit 0")
    for problem in problems:
        print("FAIL", problem)
    print("FRAMES_TOOL=" + ("FAIL" if problems else "PASS"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
