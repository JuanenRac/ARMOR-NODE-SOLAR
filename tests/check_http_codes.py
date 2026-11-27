#!/usr/bin/env python3
# ARMOR-NODE-SOLAR - guard on the status codes of the panel's server. Nothing is compiled: core/http_status.hpp is the one table, and this fails when
#   1. the server (main/web_server.cpp, main/api_shared.cpp) answers with a code that is not in the table, which would go out as a 500 with the right body;
#   2. the stand-in node the panel is tried against (tools/panel_mock.mjs) answers with a code that is not in the table, so a page could be tried against an answer the node never gives;
#   3. a code in the table has no status line (the table and the switch must say the same);
#   4. an answer is sent while the lock of the sessions is held: a slow browser then holds up every request that carries a session, on both servers.
# Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def read(relative: str) -> str:
    path = ROOT / relative
    return path.read_text(encoding="utf-8", errors="replace") if path.is_file() else ""


def problems_of(table: str, sources: dict[str, str], mock: str) -> list[str]:
    problems: list[str] = []
    listed = re.search(r"kKnownCodes\[\]\s*=\s*\{([^}]*)\}", table)
    if not listed:
        return ["core/http_status.hpp has no kKnownCodes"]
    known = {int(code) for code in re.findall(r"\d+", listed.group(1))}
    lines = set(int(code) for code in re.findall(r'case (\d{3}): return "\1 ', table))
    for code in sorted(known - lines - {500}):
        problems.append(f"the code {code} is in kKnownCodes and has no status line")
    for code in sorted(lines - known):
        problems.append(f"the code {code} has a status line and is not in kKnownCodes")
    for name, text in sources.items():
        used = {int(code) for code in re.findall(r"\b(?:send_error|send_json|send_text)\(\s*\w+,\s*(\d{3})\b", text)}
        for code in sorted(used - known):
            problems.append(f"{name} answers {code}, which is not in core/http_status.hpp")
    answered = {int(code) for code in re.findall(r"\bjson\(\s*response,\s*(\d{3})\b", mock)} | {int(code) for code in re.findall(r"writeHead\(\s*(\d{3})\b", mock)}
    for code in sorted(answered - known - {200}):
        problems.append(f"tools/panel_mock.mjs answers {code}, which the node never gives (it is not in core/http_status.hpp)")
    return problems


def answers_under_lock(server: str) -> list[str]:
    """Every block that takes `g_lock` is read from the guard to the brace that closes it: nothing in it may send."""
    problems: list[str] = []
    code = re.sub(r"//[^\n]*", "", server)
    for guard in re.finditer(r"lock_guard<std::mutex>\s+\w+\s*\(\s*g_lock\s*\)", code):
        depth, at = 0, guard.end()
        while at < len(code):
            if code[at] == "{":
                depth += 1
            elif code[at] == "}":
                if depth == 0:
                    break
                depth -= 1
            at += 1
        held = code[guard.end():at]
        for call in sorted(set(re.findall(r"\b(send_\w+|httpd_resp_send\w*)\s*\(", held))):
            line = code.count("\n", 0, guard.start()) + 1
            problems.append(f"main/web_server.cpp:{line}: {call}() is called while the lock of the sessions is held")
    return problems


def main() -> int:
    problems = problems_of(read("core/http_status.hpp"), {name: read(name) for name in ("main/web_server.cpp", "main/api_shared.cpp")}, read("tools/panel_mock.mjs"))
    problems += answers_under_lock(read("main/web_server.cpp"))
    if problems:
        print("HTTP_CODES=FAIL")
        for problem in problems:
            print("  " + problem)
        return 1
    print("HTTP_CODES=PASS the server and the stand-in node answer only with the codes of the table")
    return 0


if __name__ == "__main__":
    sys.exit(main())
