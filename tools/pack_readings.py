#!/usr/bin/env python3
"""ARMOR-NODE-SOLAR - compresses the style sheet of the readings page for embedding in the firmware.
Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.

    pack_readings.py SOURCE.css TARGET.gz

The compression is reproducible (no timestamp), as tools/pack_panel.py does for the rest of the panel.
"""
import gzip
import pathlib
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    source, target = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    if not source.is_file():
        print(f"missing panel file: {source}", file=sys.stderr)
        return 1
    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open("wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, compresslevel=9, mtime=0) as packed:
            packed.write(source.read_bytes())
    return 0


if __name__ == "__main__":
    sys.exit(main())
