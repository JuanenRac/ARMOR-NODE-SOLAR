#!/usr/bin/env python3
"""Check that what the interrupt handlers of the soft serial ports touch lives in internal memory.

Usage:  python tests/check_isr_memory.py

With the external RAM used by the allocator, a request larger than the internal threshold is served from the external RAM, which an interrupt handler that runs while the flash cache is off
must never touch. The ring of edges that the receive interrupt writes and the copy of the bits that the transmit timer reads are therefore allocated with MALLOC_CAP_INTERNAL, and the
ring is not an array inside the object that is created with a plain `new`. This checks the text of main/uart_ports.cpp (no build needed).
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
    text = (ROOT / "main" / "uart_ports.cpp").read_text(encoding="utf-8")
    problems: list[str] = []
    shared = re.search(r"struct SoftShared \{(.*?)\n\};", text, re.S)
    if shared is None:
        problems.append("SoftShared was not found")
    elif re.search(r"EdgeRecord\s+ring\s*\[", shared.group(1)):
        problems.append("the ring of edges is an array inside SoftShared: it would be allocated with the object, possibly in the external RAM")
    if not re.search(r"ring\s*=\s*static_cast<EdgeRecord\*>\(heap_caps_calloc\([^;]*MALLOC_CAP_INTERNAL", text):
        problems.append("the ring of edges is not allocated with MALLOC_CAP_INTERNAL")
    if not re.search(r"bool\* copy = static_cast<bool\*>\(heap_caps_malloc\([^;]*MALLOC_CAP_INTERNAL", text):
        problems.append("the copy of the bits that the transmit timer reads is not in internal memory")
    if "IRAM_ATTR on_rx_edge" not in text:
        problems.append("the receive-edge handler is not in IRAM")
    for problem in problems:
        print("FAIL", problem)
    print("isr memory:", "FAILED" if problems else "ok")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
