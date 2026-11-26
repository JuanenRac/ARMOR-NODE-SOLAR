#!/usr/bin/env python3
"""The node's protocol table (core/solar_protocols.hpp) says what ARMOR-COMMON's catalogue says (src/armor_common/solar_protocols.json).

Usage:  python tests/check_protocols.py

Skipped when ARMOR-COMMON is not next to this repository, or does not have the catalogue yet.
Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def node_table(text):
    return {m.group(1): (m.group(2), m.group(3), int(m.group(4)), m.group(5))
            for m in re.finditer(r'\{"([a-z0-9-]+)", "([a-z]+)", "([a-z0-9-]*)", (\d+), "(inverter|battery)"\}', text)}


def main() -> int:
    table = node_table((ROOT / "core" / "solar_protocols.hpp").read_text(encoding="utf-8"))
    if len(table) < 10:
        print("PROTOCOLS=FAIL the table of core/solar_protocols.hpp could not be read")
        return 1
    shared = ROOT.parent / "ARMOR-COMMON" / "src" / "armor_common" / "solar_protocols.json"
    if not shared.is_file():
        print(f"PROTOCOLS=SKIP {len(table)} protocols; ARMOR-COMMON has no catalogue next to this repository")
        return 0
    catalogue = {p["id"]: (p["port_kind"], p["dialect"], p["baud"], p["message"]) for p in json.loads(shared.read_text(encoding="utf-8"))["protocols"]}
    problems = [f"{name}: node {table.get(name)} / catalogue {catalogue.get(name)}" for name in sorted(set(table) | set(catalogue)) if table.get(name) != catalogue.get(name)]
    for problem in problems:
        print("PROTOCOLS=FAIL " + problem)
    if not problems:
        print(f"PROTOCOLS=PASS {len(table)} protocols as in the shared catalogue")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
