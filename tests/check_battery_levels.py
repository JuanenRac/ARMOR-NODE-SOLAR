#!/usr/bin/env python3
"""Check the Readings page's battery states: the three levels, the cells that are not read and the blink of a cell of poor health.

Usage:  python tests/check_battery_levels.py

The classification is run with Node.js when it is there (as on the CI runners); the style rules are read as text.
"""
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CASES = [({"column": "voltage", "word": "High"}, 1), ({"column": "cell_voltage", "word": "Low"}, 1), ({"column": "current", "word": "DOCAlm"}, 1),
         ({"column": "system", "word": "High"}, 2), ({"column": "voltage", "word": "OV"}, 2), ({"column": "voltage", "word": "UV"}, 2),
         ({"column": "current", "word": "COC"}, 2), ({"column": "temperature", "word": "OT"}, 2), ({"column": "voltage", "word": "NewWord"}, 2),
         ({"column": "base", "word": "SysError"}, 3)]


def main() -> int:
    problems = []
    css = (ROOT / "panel" / "readings.css").read_text(encoding="utf-8")
    app = (ROOT / "panel" / "app.js").read_text(encoding="utf-8")
    blink = re.search(r"animation: rv-soh ([0-9.]+)s", css)
    if not blink or float(blink.group(1)) < 0.5:
        problems.append("a cell of poor health must blink, no faster than twice a second")
    if not re.search(r"prefers-reduced-motion: reduce\) \{ \.rv-cell\.soh \{ animation: none;", css):
        problems.append("the blink must stop for a person who asked for less movement")
    for needed in (".rv-cell.soh .rv-tube i", ".rv-cell.soh small::after"):
        if needed not in css:
            problems.append(f"readings.css has no {needed} rule (red bar, and a mark that is not only a colour)")
    if 't("rv_sohBad")' not in app:
        problems.append("the title of a cell of poor health must say it")
    line = next((row for row in app.splitlines() if row.startswith("const flagLevel = ")), "")
    if not line:
        problems.append("app.js has no flagLevel")
    node = shutil.which("node")
    if line and node:
        script = line + "\nprocess.stdout.write(JSON.stringify(" + json.dumps([case for case, _ in CASES]) + ".map(flagLevel)));"
        got = json.loads(subprocess.run([node, "-e", script], capture_output=True, text=True, check=True).stdout)
        for (case, want), level in zip(CASES, got, strict=True):
            if level != want:
                problems.append(f"{case['column']} {case['word']} is level {level}, not {want}")
    for problem in problems:
        print("  " + problem)
    print("BATTERY_LEVELS=" + ("FAIL" if problems else "PASS") + ("" if node else " (Node.js not found: the levels were not run)"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
