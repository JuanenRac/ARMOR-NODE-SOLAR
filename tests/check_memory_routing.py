#!/usr/bin/env python3
"""What keeps the internal memory free with several battery stacks stays in place: the port tasks keep their data in the external memory, and the readings are never joined in one text.

Usage:  python tests/check_memory_routing.py        (reads main/, no build)
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def body(text: str, start: str) -> str:
    """The text of the function that begins with `start`, up to its closing brace at the left margin."""
    at = text.find(start)
    if at < 0:
        return ""
    end = text.find("\n}\n", at)
    return text[at:end if end >= 0 else len(text)]


def read(name: str) -> str:
    return (ROOT / name).read_text(encoding="utf-8")


def problems() -> list:
    found = []
    manager, web, api, build = read("main/solar_manager.cpp"), read("main/web_server.cpp"), read("main/api_shared.cpp"), read("main/CMakeLists.txt")
    for task, opened in (("void port_task(", "ports::open("), ("void group_task(", "muxboard::open_group(")):
        name = task[5:-1]
        text = body(manager, task)
        keep, port = text.find("memory::keep_data_in_psram()"), text.find(opened)
        if keep < 0:
            found.append(f"{name} does not keep its data in the external memory")
            continue
        if port < 0 or keep < port:
            found.append(f"{name} must ask for the external memory AFTER its port is open (the interrupts read the port)")
        if text.find("std::make_unique<solar::Poller>") < keep:
            found.append(f"{name} makes its poller before it asked for the external memory")
    for name in ("std::string status_json()", "std::string ports_json()"):
        if "memory::ExternalScope" not in body(api, name):
            found.append(f"{name} builds a long answer outside the external memory")
    if '"reading"' in api:
        found.append("api_shared.cpp puts the readings inside another answer again: they go out on their own, port by port (/api/v1/readings)")
    readings = body(web, "esp_err_t get_readings(")
    if "httpd_resp_send_chunk" not in readings or "+=" in readings:
        found.append("get_readings must send the readings in chunks, one port at a time, without joining them")
    if '"psram_new.cpp"' not in build:
        found.append("main/CMakeLists.txt does not build psram_new.cpp")
    return found


def main() -> int:
    found = problems()
    for problem in found:
        print("FAIL:", problem)
    if not found:
        print("memory routing: ok")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
