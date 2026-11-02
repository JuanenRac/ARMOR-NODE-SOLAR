#!/usr/bin/env python3
"""Writes docs/COMPATIBILITY.md from docs/compatibility.json (the table of the inverters and batteries this node reads, and how far each has been verified).

Usage:  python tools/make_compatibility.py            # write the page
        python tools/make_compatibility.py --check    # fail when the page is not what the table says (used by the tests)

The table is the source: a row moves from "protocol" or "capture" to "bench" only when the model, the firmware and the commands that answered are written in its `bench` member.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "docs" / "compatibility.json"
PAGE = ROOT / "docs" / "COMPATIBILITY.md"

TITLES = {"bench": "Bench", "capture": "Capture", "protocol": "Protocol"}


def cell(text: str) -> str:
    return text.replace("|", "\\|").replace("\n", " ")


def render(document: dict) -> str:
    lines = [
        "# Compatibility: which inverters and batteries this node reads, and how far each is verified",
        "",
        "Generated from [compatibility.json](compatibility.json) by `tools/make_compatibility.py`; edit the table, not this page.",
        "",
        "The node reads these devices; the setting commands of the inverters are never built, and writing to a battery's console is a separate capability that is off by default. A row has one of three levels:",
        "",
    ]
    for key in ("bench", "capture", "protocol"):
        lines.append(f"- **{TITLES[key]}**: {document['levels'][key]}.")
    lines += [
        "",
        "**No inverter and no battery has been connected to this node yet**, so every row below is still pending on the bench. At the bench, for each model: connect it, read the model and the firmware it",
        "reports, note which of the commands in the *Reads* column answered, and write the three in the row's `bench` member (`model`, `firmware`, `answers`) with `status` set to `done` and the level set to",
        "`bench`. The test that checks this table refuses a `bench` row that has no numbers, and a pending row that claims the level.",
        "",
    ]
    for kind, title in (("inverter", "Inverters"), ("battery", "Batteries")):
        lines += [f"## {title}", "", "| Model | Port | Reads | Level | Evidence | Bench |", "| --- | --- | --- | --- | --- | --- |"]
        for row in document["rows"]:
            if row["kind"] != kind:
                continue
            bench = row["bench"]
            bench_text = f"{bench['model']} / {bench['firmware']} / {bench['answers']}" if bench["status"] == "done" else "pending"
            lines.append(f"| {cell(row['name'])} | {cell(row['port'])} | {cell(row['reads'])} | {TITLES[row['level']]} | {cell(row['evidence'])} | {cell(bench_text)} |")
        lines.append("")
    return "\n".join(lines)


def main() -> int:
    document = json.loads(SOURCE.read_text(encoding="utf-8"))
    text = render(document)
    if "--check" in sys.argv:
        current = PAGE.read_text(encoding="utf-8").replace("\r\n", "\n") if PAGE.is_file() else ""
        if current != text:
            print(f"FAIL {PAGE.relative_to(ROOT)} is not what compatibility.json says: run python tools/make_compatibility.py")
            return 1
        print(f"COMPATIBILITY=PASS {len(document['rows'])} rows")
        return 0
    PAGE.write_text(text, encoding="utf-8", newline="\n")
    print(f"written {PAGE.relative_to(ROOT)} ({len(document['rows'])} rows)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
