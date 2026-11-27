#!/usr/bin/env python3
"""The equipment and protocol lists of the panel offer every value the firmware knows, once, under a titled group.

Usage:  python tests/check_equipment_options.py

The lists are arranged for a person (groups, brand order), so nothing but this test says that no value was lost in the arranging.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GROUPS = ["groupInverters", "groupBatteries", "groupOther"]


def main():
    config = (ROOT / "core" / "solar_config.hpp").read_text(encoding="utf-8")
    app = (ROOT / "panel" / "app.js").read_text(encoding="utf-8")
    text = (ROOT / "panel" / "text.js").read_text(encoding="utf-8")
    problems = []
    firmware = re.findall(r'\{Kind::k\w+, "(\w+)"', config)
    block = app[app.find("const KINDS = ["):app.find("const kindOf")]
    panel = re.findall(r'\["(\w+)", "kind_\w+", \d+\]', block)
    if sorted(panel) != sorted(firmware) or len(panel) != len(set(panel)):
        problems.append(f"the equipment list of the panel {sorted(panel)} is not the kinds of the firmware {sorted(firmware)}")
    grouped = []
    group_block = app[app.find("const KIND_GROUPS = ["):app.find("];", app.find("const KIND_GROUPS = ["))]
    for title, ids in re.findall(r'\["(\w+)", \[([^\]]*)\]\]', group_block):
        grouped += re.findall(r'"(\w+)"', ids)
        if title not in GROUPS:
            problems.append(f"unknown group {title}")
    if sorted(grouped) != sorted(firmware):
        problems.append(f"the groups hold {sorted(grouped)}, not every kind exactly once")
    for title in GROUPS:
        row = re.search(rf'^  {title}: \[(.*)\],$', text, re.M)
        if not row or len(re.findall(r'"(?:[^"\\]|\\.)*"', row.group(1))) != 8:
            problems.append(f"{title} needs its eight texts")
    for kind, enum in (("voltronic", "kVoltronic"), ("modbus", "kModbus"), ("vedirect", "kVedirect")):
        valid = re.search(rf"Kind::{enum}\) return among\(\{{([^}}]*)\}}\)", config)
        offered = re.search(rf'cfg\.kind === "{kind}" \? field\("dialectLabel".*?options: \[(.*?)\], hint:', app)
        if not valid or not offered:
            problems.append(f"the protocol list of {kind} was not found")
            continue
        want = re.findall(r'"([\w-]+)"', valid.group(1))
        have = re.findall(r'\["([\w-]+)",', offered.group(1))
        if sorted(have) != sorted(want) or have[0] != "auto":
            problems.append(f"the protocol list of {kind} {have} must start with auto and hold {want}")
    for problem in problems:
        print("FAIL:", problem)
    print("equipment options: " + ("FAILED" if problems else "OK"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
