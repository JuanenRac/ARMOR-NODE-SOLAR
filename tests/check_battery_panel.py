#!/usr/bin/env python3
# ARMOR-NODE-SOLAR - guard on writing to a battery from the node's own panel. Nothing is compiled: it reads the sources and fails when
#   1. the page and the node's table of operations or limits differ (panel/maintenance.js and core/battery_ops.hpp);
#   2. a refusal of the node has no sentence on the page, or a row of the page lacks one of the eight languages;
#   3. a lock is missing in main/web_server.cpp: HTTPS, the administrator, the password asked again, the writes turned on, no restart or firmware change in the middle of an operation;
#   4. the one-time token of an arm could reach the browser.
# Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def read(name: str) -> str:
    return (ROOT / name).read_text(encoding="utf-8").replace("\r\n", "\n")


def body(text: str, start: str) -> str:
    at = text.find(start)
    if at < 0:
        return ""
    end = text.find("\n}\n", at)
    return text[at:end if end >= 0 else len(text)]


def rows(text: str) -> dict[str, list[str]]:
    return {m.group(1): re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(2)) for m in re.finditer(r"^  ([A-Za-z0-9_]+): \[(.*)\],?$", text, re.M)}


def problems_of(ops: str, page: str, texts: str, web: str, node: str) -> list[str]:
    found: list[str] = []
    table = rows(texts)
    table.update(rows(page))
    for key, strings in rows(page).items():
        if len(strings) != 8 or any(not s.strip() for s in strings):
            found.append(f"{key}: not eight texts")

    wanted = {name for name in re.findall(r'\{Op::k\w+, "(\w+)"', ops)} - {"preflight"}
    groups = page[page.find("const BM_GROUPS"):page.find("];\n", page.find("const BM_GROUPS"))]
    offered: set[str] = set()
    for listed in re.findall(r'\["\w+", \[([^\]]*)\]\]', groups):
        offered.update(re.findall(r'"(\w+)"', listed))
    if offered != wanted:
        found.append(f"the page offers other operations than the node has: {sorted(offered ^ wanted)}")
    for op in sorted(wanted):
        for key in (f"bm_op_{op}", f"bm_opd_{op}"):
            if key not in table:
                found.append(f"no text {key}")

    limits = re.findall(r'\{"(\w+)", (-?\d+), (-?\d+),\n\s*\{((?:\{"\w+", "\w+"\},? ?)+)\}', ops)
    if len(limits) != 3:
        found.append("the node's table of limits could not be read")
    for name, low, high, keys in limits:
        shown = re.search(r'name: "' + name + r'", lowest: (-?\d+), highest: (-?\d+), unit: "[^"]+", keys: \[([^\]]*)\]', page)
        if not shown:
            found.append(f"the page has no limit group {name}")
        elif (shown.group(1), shown.group(2)) != (low, high) or re.findall(r'"(\w+)"', shown.group(3)) != [k for k, _ in re.findall(r'\{"(\w+)", "(\w+)"\}', keys)]:
            found.append(f"the limit group {name} of the page differs from the node's")

    refusals = re.findall(r'constexpr const char\* k\w+ = "(\w+)";', ops)
    for code in refusals:
        if code != "none" and not any(prefix + code in table for prefix in ("bm_ref_", "bm_err_", "bc_code_", "e_")) and not code.startswith("stage_") and code != "disabled":
            found.append(f"the refusal {code} has no sentence on the page")
    for code in ("https_required", "battery_write_disabled", "no_password", "node_paused", "wrong_password", "too_many_attempts", "too_many_orders", "campaign_open", "campaign_state", "serial_repeated",
                 "module_differs", "battery_busy", "image_header", "bad_size", "upload_interrupted", "upload_stalled"):
        if not any(prefix + code in table for prefix in ("bm_ref_", "bm_err_", "bc_code_", "e_")):
            found.append(f"the code {code} has no sentence on the page")

    gate = body(web, "bool battery_gate(")
    for needle in ("require(r, who, true, true)", "is_tls(r)", "panel_flags(", "node_control::working()", "gate_refusal("):
        if needle not in gate:
            found.append(f"battery_gate lacks {needle}")
    for handler in ("esp_err_t post_battery(", "esp_err_t post_battery_image("):
        if "battery_gate(r, who)" not in body(web, handler):
            found.append(f"{handler} does not pass the gate")
    post = body(web, "esp_err_t post_battery(")
    if 'step != "cancel" && !password_again(' not in post:
        found.append("the password is not asked again before an order")
    again = body(web, "bool password_again(")
    for needle in ("store::user_verify(", "g_user_throttle.allowed(", "g_user_throttle.failure("):
        if needle not in again:
            found.append(f"password_again lacks {needle}")
    for handler in ("esp_err_t get_battery(", "esp_err_t get_battery_audit("):
        text = body(web, handler)
        if "require(r, who, true, false)" not in text or "is_tls(r)" not in text:
            found.append(f"{handler} must be for an administrator over HTTPS")
    for handler in ("post_reboot", "post_ota_switch", "post_ota_copy", "post_factory_reset", "post_ota", "post_ota_install"):
        if "maintenance::busy()" not in body(web, f"esp_err_t {handler}("):
            found.append(f"{handler} does not refuse while a battery operation is under way")

    if "reply_json(r, false)" not in node:
        found.append("the answer kept for the panel must not carry the token")
    if re.search(r'field\("token"', body(node, "std::string panel_state(")):
        found.append("panel_state gives the token away")
    if re.search(r"\.token\b|\btoken\b *:", page):
        found.append("the page handles a token: it never leaves the node")
    if "heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)" not in body(node, "std::shared_ptr<std::vector<std::uint8_t>> panel_image_buffer("):
        found.append("an uploaded image must only be taken when the external memory holds it")
    state = body(node, "std::string panel_state(")
    if not 0 <= state.find("if (!enabled)") < state.find("panel_data()"):
        found.append("panel_state makes the panel's memory with the writes off")
    return found


def main() -> int:
    problems = problems_of(read("core/battery_ops.hpp"), read("panel/maintenance.js"), read("panel/text.js"), read("main/web_server.cpp"), read("main/battery_maintenance.cpp"))
    if problems:
        print("BATTERY_PANEL=FAIL")
        for problem in problems:
            print("  " + problem)
        return 1
    print("BATTERY_PANEL=PASS the page and the node agree, every refusal has its sentence in eight languages, the locks are in place and the token stays in the node")
    return 0


if __name__ == "__main__":
    sys.exit(main())
