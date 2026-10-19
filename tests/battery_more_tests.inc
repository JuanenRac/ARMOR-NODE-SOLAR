// More host tests of the battery write capability (included by test_battery_write.cpp): the staged image, the setting and the read-only question box.

// ---- the staged image ---------------------------------------------------------------------------------------------------------------------------------

static std::string b64(const std::uint8_t* d, std::size_t n) {
  static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  for (std::size_t i = 0; i < n; i += 3) {
    const std::uint32_t v = (static_cast<std::uint32_t>(d[i]) << 16) | (i + 1 < n ? static_cast<std::uint32_t>(d[i + 1]) << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
    o += t[(v >> 18) & 63]; o += t[(v >> 12) & 63]; o += i + 1 < n ? t[(v >> 6) & 63] : '='; o += i + 2 < n ? t[v & 63] : '=';
  }
  return o;
}

static void test_the_staged_image() {
  std::vector<std::uint8_t> out;
  CHECK(base64_decode("TWFu", out) && out.size() == 3 && out[0] == 'M' && out[1] == 'a' && out[2] == 'n');
  CHECK(base64_decode("TWE=", out) && out.size() == 2 && base64_decode("TQ==", out) && out.size() == 1);
  for (const char* bad : {"", "TWF", "TW=u", "T===", "TWFu\n", "TW Fu", "TWE=TWFu", "TQ=A", "TWF*"}) CHECK(!base64_decode(bad, out));
  const std::vector<std::uint8_t> image = bench::make_image("US2000C", 2, 4, 100'000);
  const std::uint32_t crc = crc32(image.data(), image.size());
  const std::size_t total = (image.size() + 255) / 256;
  ImageStage stage;
  std::string why;
  for (std::size_t i = 0; i < total; ++i) {
    const std::size_t n = std::min<std::size_t>(256, image.size() - i * 256);
    why = stage.add("abcdef0123456789", i, total, image.size(), crc, b64(image.data() + i * 256, n), 1000 + i);
    CHECK(why.empty());
    CHECK(stage.ready() == (i + 1 == total));
  }
  CHECK(stage.ready() && stage.image() && *stage.image() == image && stage.received() == image.size());
  stage.clear();
  CHECK(!stage.ready() && !stage.image());
  // a wrong CRC, a piece out of order, a short piece in the middle, too much data: refused, and nothing is kept
  {
    ImageStage s;
    for (std::size_t i = 0; i < total; ++i) { const std::size_t n = std::min<std::size_t>(256, image.size() - i * 256); why = s.add("abcdef0123456789", i, total, image.size(), crc ^ 1, b64(image.data() + i * 256, n), 0); }
    CHECK(why == "stage_crc" && !s.ready() && !s.image());
    ImageStage o;
    CHECK(o.add("abcdef0123456789", 0, total, image.size(), crc, b64(image.data(), 256), 0).empty());
    CHECK(o.add("abcdef0123456789", 2, total, image.size(), crc, b64(image.data() + 512, 256), 0) == "stage_order");
    CHECK(o.add("abcdef0123456789", 1, total, image.size() + 1, crc, b64(image.data() + 256, 256), 0) == "stage_order");
    CHECK(o.add("otherid0123456789", 1, total, image.size(), crc, b64(image.data() + 256, 256), 0) == "stage_order");
    CHECK(o.add("abcdef0123456789", 1, total, image.size(), crc, b64(image.data() + 256, 100), 0) == "stage_data");
    CHECK(o.add("abcdef0123456789", 1, total, image.size(), crc, "!!!!", 0) == "stage_data");
    ImageStage fresh;
    CHECK(fresh.add("abcdef0123456789", 1, total, image.size(), crc, b64(image.data(), 256), 0) == "stage_order");   // no first piece
    CHECK(fresh.add("short", 0, total, image.size(), crc, b64(image.data(), 256), 0) == "stage_invalid");
    CHECK(fresh.add("abcdef0123456789", 0, total, 10, crc, b64(image.data(), 256), 0) == "stage_invalid");           // below the smallest image
    CHECK(fresh.add("abcdef0123456789", 0, 5, image.size(), crc, b64(image.data(), 256), 0) == "stage_invalid");     // the pieces cannot make this size
    CHECK(fresh.add("abcdef0123456789", 0, 3000, 600'000, crc, b64(image.data(), 256), 0) == "stage_invalid");
    // a new first piece drops the old image; an old image expires
    CHECK(o.add("abcdef0123456789", 0, total, image.size(), crc, b64(image.data(), 256), 0).empty() && o.received() == 256);
    o.expire(ImageStage::kKeepMs + 10);
    CHECK(o.received() == 0 && !o.ready());
  }
}

// ---- the setting and the read-only console ----------------------------------------------------------------------------------------------------------

static void test_the_setting() {
  config::Settings s = config::default_settings("a1b2c3");
  s.sta.enabled = true; s.sta.ssid = "home"; s.sta.password = "home-password";
  CHECK(!s.battery.enabled && s.battery.service_password.empty());   // off by default
  CHECK(config::validate(s).empty());
  config::Settings out; config::Problems problems;
  CHECK(!config::load("{\"battery\":{\"write_enabled\":true}}", s, out, problems));              // on, but no password
  bool needs_password = false;
  for (const auto& p : problems) if (p.path == "battery.service_password" && p.code == "required") needs_password = true;
  CHECK(needs_password);
  problems.clear();
  CHECK(config::load("{\"battery\":{\"write_enabled\":true,\"service_password\":\"dummy-service-pw\"}}", s, out, problems) && out.battery.enabled && out.battery.service_password == "dummy-service-pw");
  // the panel never gets the password; the flash does
  const std::string panel = config::to_json(out, false), flash = config::to_json(out, true);
  CHECK(panel.find("dummy-service-pw") == std::string::npos && panel.find("\"service_password_set\":true") != std::string::npos);
  CHECK(flash.find("\"service_password\":\"dummy-service-pw\"") != std::string::npos);
  // a document that comes back from the panel without the password keeps it; the clear flag erases it
  config::Settings kept; problems.clear();
  CHECK(config::load("{\"battery\":{\"write_enabled\":true,\"service_password\":\"\"}}", out, kept, problems) && kept.battery.service_password == "dummy-service-pw");
  config::Settings cleared; problems.clear();
  CHECK(config::load("{\"battery\":{\"write_enabled\":false,\"service_password_clear\":true}}", out, cleared, problems) && cleared.battery.service_password.empty() && !cleared.battery.enabled);
  problems.clear();
  CHECK(!config::load("{\"battery\":{\"write_enabled\":true,\"service_password\":\"has space\"}}", s, out, problems));
  problems.clear();
  CHECK(!config::load("{\"battery\":{\"write_enabled\":\"yes\"}}", s, out, problems));
  problems.clear();
  CHECK(!config::load("{\"battery\":{\"write_enabled\":true,\"service_password\":\"" + std::string(200, 'a') + "\"}}", s, out, problems));
}

static void test_the_read_only_console_stays_read_only() {
  std::string canonical;
  // every writing or dangerous word of the console is refused by the question box, with or without an argument
  for (const char* name : {"ctrl", "prot", "config", "shut", "setprt", "urates", "eepw", "eepe", "eepr", "flaw", "flae", "mm", "login", "logout", "logc", "statc", "datac", "save", "re", "m2s", "totalcoul", "couls", "setlowcfg", "emptyvolt", "blocksoc",
                           "tbar", "tmbar", "trst", "updata", "time", "rtcw", "socw", "soccfg", "soccal", "socclr", "tmos", "tled", "tbld", "tbuz", "tflash", "twdg"}) {
    CHECK(!console::allowed(name, canonical));
    CHECK(!console::allowed(std::string(name) + " 1", canonical));
    CHECK(!console::allowed(std::string(name) + " on", canonical));
  }
  CHECK(console::allowed("info 1", canonical) && canonical == "info 1");
  // and nothing a typed operation writes is something the question box would let through
  bench::Battery b;
  Plan plan; std::string why;
  Request r = request(Op::kOutputSet, "{\"output\":\"cfet\",\"state\":\"on\"}", identity_of(b));
  CHECK(build_plan(r, plan, why));
  for (const Step& st : plan.steps) if (st.write) CHECK(!console::allowed(st.send, canonical));
}
