// ARMOR-NODE-SOLAR - host test of the compatibility table (docs/compatibility.json): the shape of every row, the three levels, that a row only says "bench" when the bench numbers are
// written in it, and that the tests a row points at exist. The table is what the bench fills in: a row stays "pending" until a real unit has been read.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// Usage: test_compat [path of docs/compatibility.json]
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include <string>

#include "../core/json.hpp"

static int failures = 0;
static int checks = 0;
#define CHECK(condition)                                                              \
  do {                                                                                \
    ++checks;                                                                         \
    if (!(condition)) {                                                               \
      ++failures;                                                                     \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);                \
    }                                                                                 \
  } while (0)

using namespace armor;

static bool file_exists(const std::string& path) { return std::ifstream(path).good(); }

// Checks one document and returns how many things are wrong with it; the rows' tests are looked for under `root` (the repository). Nothing is printed: the bad documents of the test below are
// expected to be wrong.
static int check_document(const json::Value& d, const std::string& root, bool announce) {
  int problems = 0;
  const auto need = [&](bool condition) { if (!condition) ++problems; };
  need(d.is_object() && d.integer_or("schema", 0, 0, 100) == 1);
  const json::Value* levels = d.get("levels");
  need(levels != nullptr && levels->is_object() && levels->get("bench") && levels->get("capture") && levels->get("protocol") && levels->names.size() == 3);
  const json::Value* rows = d.get("rows");
  need(rows != nullptr && rows->is_array() && !rows->items.empty());
  if (rows == nullptr || !rows->is_array()) return problems;
  std::set<std::string> ids;
  int inverters = 0, batteries = 0, bench = 0, capture = 0, protocol = 0;
  for (const json::Value& row : rows->items) {
    const std::string id = row.string_or("id", "");
    need(!id.empty() && ids.insert(id).second);   // every row has its own name
    const std::string kind = row.string_or("kind", "");
    need(kind == "inverter" || kind == "battery");
    (kind == "inverter" ? inverters : batteries) += 1;
    need(!row.string_or("name", "").empty() && !row.string_or("port", "").empty() && !row.string_or("reads", "").empty() && !row.string_or("evidence", "").empty());
    const std::string level = row.string_or("level", "");
    need(level == "bench" || level == "capture" || level == "protocol");
    (level == "bench" ? bench : level == "capture" ? capture : protocol) += 1;
    const json::Value* tests = row.get("tests");
    need(tests != nullptr && tests->is_array() && !tests->items.empty());
    if (tests != nullptr && tests->is_array() && !root.empty()) for (const json::Value& t : tests->items) need(t.is_string() && file_exists(root + "/" + t.text));
    // the bench: pending until a real unit has been read; then the model, the firmware and what answered are written, and only then is the level "bench"
    const json::Value* b = row.get("bench");
    need(b != nullptr && b->is_object());
    if (b == nullptr) continue;
    const std::string status = b->string_or("status", "");
    need(status == "pending" || status == "done");
    const bool written = !b->string_or("model", "").empty() && !b->string_or("firmware", "").empty() && !b->string_or("answers", "").empty();
    if (status == "done") need(written && level == "bench");
    else need(!written || level != "bench");   // a pending row cannot claim the bench level
    if (level == "bench") need(status == "done");
  }
  if (announce) std::printf("%zu rows: %d inverter, %d battery; %d bench, %d capture, %d protocol\n", rows->items.size(), inverters, batteries, bench, capture, protocol);
  need(inverters >= 1 && batteries >= 1);
  return problems;
}

static void test_a_bad_document_is_seen() {
  const auto bad = [&](const std::string& text) { json::Value v; return !json::parse(text, v) || check_document(v, "", false) > 0; };
  const std::string row = "{\"id\":\"a\",\"kind\":\"battery\",\"name\":\"n\",\"port\":\"p\",\"reads\":\"r\",\"level\":\"protocol\",\"evidence\":\"e\",\"tests\":[\"t\"],\"bench\":{\"status\":\"pending\",\"model\":\"\",\"firmware\":\"\",\"answers\":\"\"}}";
  const std::string levels = "\"levels\":{\"bench\":\"b\",\"capture\":\"c\",\"protocol\":\"p\"}";
  const std::string inverter = "{\"id\":\"i\",\"kind\":\"inverter\",\"name\":\"n\",\"port\":\"p\",\"reads\":\"r\",\"level\":\"protocol\",\"evidence\":\"e\",\"tests\":[\"t\"],\"bench\":{\"status\":\"pending\",\"model\":\"\",\"firmware\":\"\",\"answers\":\"\"}}";
  const auto doc = [&](const std::string& rows) { return "{\"schema\":1," + levels + ",\"rows\":[" + rows + "]}"; };
  CHECK(!bad(doc(row + "," + inverter)));                                                       // a good one passes
  CHECK(bad(doc(row)));                                                                          // no inverter
  CHECK(bad(doc(row + "," + row)));                                                              // the same id twice
  std::string x = row; x.replace(x.find("\"protocol\""), 10, "\"verified\"");
  CHECK(bad(doc(x + "," + inverter)));                                                           // a level that does not exist
  x = row; x.replace(x.find("\"pending\""), 9, "\"done\"");
  CHECK(bad(doc(x + "," + inverter)));                                                           // done with nothing written
  x = row; x.replace(x.find("\"protocol\""), 10, "\"bench\"");
  CHECK(bad(doc(x + "," + inverter)));                                                           // bench level but still pending
  x = row; x.replace(x.find("\"model\":\"\""), 10, "\"model\":\"US2000C\"");
  x.replace(x.find("\"firmware\":\"\""), 13, "\"firmware\":\"V2.6\""); x.replace(x.find("\"answers\":\"\""), 12, "\"answers\":\"pwr info\"");
  x.replace(x.find("\"pending\""), 9, "\"done\""); x.replace(x.find("\"protocol\""), 10, "\"bench\"");
  CHECK(!bad(doc(x + "," + inverter)));                                                          // a bench row with its numbers is fine
  x = row; x.replace(x.find("\"tests\":[\"t\"]"), 13, "\"tests\":[]");
  CHECK(bad(doc(x + "," + inverter)));                                                           // a row with no tests
  CHECK(bad("{\"schema\":2," + levels + ",\"rows\":[]}"));
}

int main(int argc, char** argv) {
  const std::string path = argc > 1 ? argv[1] : "";
  std::ifstream file(path);
  const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  json::Value document;
  CHECK(!text.empty() && json::parse(text, document));
  // the repository is two folders up from docs/compatibility.json
  std::string root;
  const std::size_t slash = path.find_last_of("/\\");
  if (slash != std::string::npos) { root = path.substr(0, slash); const std::size_t up = root.find_last_of("/\\"); root = up == std::string::npos ? "." : root.substr(0, up); }
  CHECK(check_document(document, root, true) == 0);
  test_a_bad_document_is_seen();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
