#!/usr/bin/env python3
"""ARMOR-NODE-SOLAR - turns the capture of the exchanges of a port into a test fixture.

Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.

Turn on the capture in the panel (Ports, "Capture of the exchanges with the equipment"), let the node poll the inverter or the battery for a while, press "Download the frames" and run

    python tools/frames_to_fixture.py solar_frames.log tests/fixtures/solar_real.hex

The fixture has one entry per line ("p<port> tx|rx <hexadecimal>"): what the node sent and what the equipment answered, in order. Lines that are not whole entries (a cut line, an odd number of
hexadecimal digits) are reported and skipped. Nothing that looks like a sign-in is ever in a capture, and this tool refuses such a line too.
"""
import re
import sys

ENTRY = re.compile(r"FRAME p([0-9]{1,2}) (tx|rx) ([0-9a-f]+)\s*$")
SECRET_WORDS = (b"login", b"passw")


def convert(lines: list[str]) -> tuple[list[str], int]:
    """The entries of a capture as fixture lines, and how many lines were skipped."""
    good: list[str] = []
    skipped = 0
    for line in lines:
        if "FRAME p" not in line:
            continue
        match = ENTRY.search(line)
        if match is None or len(match.group(3)) % 2 != 0:
            skipped += 1
            continue
        raw = bytes.fromhex(match.group(3))
        if any(word in raw.lower() for word in SECRET_WORDS):
            skipped += 1
            continue
        good.append(f"p{match.group(1)} {match.group(2)} {match.group(3)}")
    return good, skipped


def main(argv: list[str]) -> int:
    if len(argv) == 2 and argv[1] in ("-h", "--help"):
        print(__doc__)
        return 0
    if len(argv) != 3:
        print(__doc__)
        return 2
    with open(argv[1], encoding="utf-8", errors="replace") as source:
        entries, skipped = convert(source.readlines())
    if not entries:
        print("no entry found: is the capture turned on and is this the file the panel downloaded?", file=sys.stderr)
        return 1
    with open(argv[2], "w", encoding="utf-8", newline="\n") as target:
        target.write("\n".join(entries) + "\n")
    print(f"{len(entries)} entries written to {argv[2]}" + (f", {skipped} lines skipped" if skipped else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
