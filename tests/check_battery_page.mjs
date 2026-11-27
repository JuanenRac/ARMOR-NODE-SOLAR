// ARMOR-NODE-SOLAR - the battery page of the panel run for real (panel/text.js, app.js and maintenance.js) against a made-up browser and made-up answers of the node: what is shown over
// plain HTTP, with the writes off, while an operation is armed, running and ended, and along a campaign; that the password goes only with an order and is then forgotten, and that the
// page never offers to execute what another session armed. In the eight languages.
//   node tests/check_battery_page.mjs
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
import { readFileSync } from "node:fs";
import path from "node:path";
import vm from "node:vm";
import { fileURLToPath } from "node:url";

const panel = path.join(path.dirname(fileURLToPath(import.meta.url)), "..", "panel");
const read = name => readFileSync(path.join(panel, name), "utf8");

class Node {
  constructor(tag) { this.tag = tag; this.kids = []; this.listeners = {}; this.attrs = {}; this.style = {}; this.className = ""; this.title = ""; this.value = ""; this.disabled = false; this.onclick = null; }
  append(...kids) { this.kids.push(...kids); }
  replaceChildren(...kids) { this.kids = kids; }
  addEventListener(name, fn) { this.listeners[name] = fn; }
  setAttribute(name, value) { this.attrs[name] = value; }
  get textContent() { return this.tag === "#text" ? this.data : this.kids.map(k => k.textContent).join(" "); }
  set textContent(v) { const n = new Node("#text"); n.data = String(v); this.kids = [n]; }
  all(tag, out = []) { if (this.tag === tag) out.push(this); for (const k of this.kids) k.all(tag, out); return out; }
}
const orders = [];
let answer = () => ({});
const sandbox = {
  console, JSON, Math, Date, Number, String, Array, Object, RegExp, Promise, Boolean, Set, Map, URLSearchParams,
  Node,
  setTimeout: () => 1, clearTimeout: () => {}, setInterval: () => 1, clearInterval: () => {},
  confirm: () => true, alert: () => {},
  localStorage: { getItem: () => null, setItem: () => {}, removeItem: () => {} },
  navigator: { language: "en" },
  location: { hostname: "node.example", hash: "#/battery", reload: () => {} },
  window: { addEventListener: () => {}, isSecureContext: true },
  XMLHttpRequest: class {},
  fetch: async (url, init) => {
    if (init.method === "POST") orders.push({ url, body: JSON.parse(init.body ?? "{}") });
    if (url.endsWith("session")) return { ok: true, status: 200, json: async () => ({ authenticated: false, setup: false }) };
    return { ok: true, status: 200, json: async () => answer(url) };
  },
  document: {
    hidden: false, documentElement: {}, head: new Node("head"), activeElement: null,
    getElementById: () => new Node("div"), createElement: tag => new Node(tag),
    createTextNode: text => { const n = new Node("#text"); n.data = String(text); return n; },
    addEventListener: () => {},
  },
};
sandbox.addEventListener = () => {};
vm.createContext(sandbox);
vm.runInContext(read("text.js") + "\n" + read("app.js") + "\n" + read("maintenance.js") + "\nthis.T = { S, B, batteryPage, bmRefresh, bmOrder, L, setLang: n => { lang = n; }, say: t };", sandbox);
const T = sandbox.T;
const wait = () => new Promise(resolve => setImmediate(resolve));

let failed = 0, checks = 0;
const check = (name, ok, extra = "") => { checks += 1; if (!ok) { failed += 1; console.log("FAIL", name, extra); } };

for (let i = 0; i < 5; i += 1) await wait();   // the panel's own start (it finds no session) runs first
T.S.session = { authenticated: true, role: "admin", user: "admin", node_id: "solar-1" };
T.S.cfg = {};
T.S.ports = [{ port: 2, enabled: true, kind: "pylontech", name: "stack" }];
const page = () => T.batteryPage();
const buttons = root => root.all("button").map(b => b.textContent.trim());
const show = async state => { answer = () => state; await T.bmRefresh(false); await wait(); return page(); };
const base = { enabled: true, password: true, https: true, port: 2, facts: { pylontech: true, present: true, current_ma: 0 }, image: null, operation: null, campaign: null };

for (let language = 0; language < 8; language += 1) {
  T.setLang(language);
  const say = key => T.say(key);

  let view = await show({ https: false, enabled: true, password: true });
  check(`plain HTTP ${language}`, view.textContent.includes(say("e_https_required")) && view.all("input").length === 0);
  view = await show({ https: true, enabled: false, password: false });
  check(`writes off ${language}`, view.textContent.includes(say("bmn_off")) && !buttons(view).includes(say("bm_prepare")));
  view = await show({ ...base, password: false });
  check(`no service password ${language}`, view.textContent.includes(say("e_no_password")) && !buttons(view).includes(say("bm_prepare")));
  view = await show(base);
  check(`ready ${language}`, buttons(view).includes(say("bm_prepare")) && view.all("input").some(i => i.attrs.type === "password") && view.all("option").length >= 20);
  check(`no campaign without an image ${language}`, view.all("button").find(b => b.textContent.trim() === say("bc_start")).disabled === true);

  const armed = { stage: "armed", op: "set_clock", user: "admin", mine: true, campaign: false, left_s: 100, reply: { risk: "low", summary: "set the clock", battery: { model: "US3000C", serial: "PPTBH01111100001", soft_version: "V3.0" } } };
  view = await show({ ...base, operation: armed });
  check(`armed ${language}`, buttons(view).includes(say("bm_execute")) && !buttons(view).includes(say("bm_prepare")) && view.textContent.includes("set the clock"));
  view = await show({ ...base, operation: { ...armed, mine: false } });
  check(`armed elsewhere ${language}`, !buttons(view).includes(say("bm_execute")) && view.textContent.includes(say("bmn_other_session")));
  view = await show({ ...base, operation: { ...armed, stage: "running", reply: { packets_sent: 5, packets_total: 100 } } });
  check(`running ${language}`, buttons(view).includes(say("bm_cancel_op")) && !buttons(view).includes(say("bm_execute")));
  view = await show({ ...base, operation: { ...armed, stage: "rejected", reply: { refusal: "preflight_failed", code: "bootloader" } } });
  check(`rejected ${language}`, view.textContent.includes(say("bc_code_bootloader")));
  view = await show({ ...base, operation: { ...armed, stage: "final", reply: { outcome: "partial", code: "firmware:battery_reported_error", transcript: "> updata" } } });
  check(`ended ${language}`, view.textContent.includes(say("bm_result_partial")) && view.textContent.includes("> updata"));

  const modules = status => [{ n: 1, serial: "PPTBH01111100001", model: "US3000C", board: "", chip: "older", status, code: "" }];
  const campaign = (state, status, extra = {}) => ({ state, port: 2, current: 0, stop_reason: "", image: { board: "US2000C", version: "3.1" }, modules: modules(status), ...extra });
  view = await show({ ...base, campaign: campaign("active", "pending") });
  check(`campaign to check ${language}`, buttons(view).includes(say("bc_check")) && !buttons(view).includes(say("bm_prepare")));
  view = await show({ ...base, campaign: campaign("active", "ready"), operation: { ...armed, op: "firmware_update", campaign: true } });
  check(`campaign to execute ${language}`, buttons(view).filter(b => b === say("bm_execute")).length === 1);
  view = await show({ ...base, campaign: campaign("stopped", "failed", { stop_reason: "chip_unknown" }) });
  check(`campaign stopped ${language}`, buttons(view).includes(say("bc_retry")) && view.textContent.includes(say("bm_ref_chip_unknown")) && !buttons(view).includes(say("bm_execute")));
  view = await show({ ...base, campaign: campaign("return_cable", "done") });
  check(`campaign cable ${language}`, buttons(view).includes(say("bc_finish")));
  check(`no raw key shows ${language}`, !/\b(bm|bmn|bc)_[a-z_]+\b/.test(view.textContent), view.textContent.match(/\b(bm|bmn|bc)_[a-z_]+\b/)?.[0]);
}

// an order carries the password once; it is then forgotten
T.setLang(0);
await show(base);
T.B.password = "a person's password";
orders.length = 0;
await T.bmOrder("battery/arm", { op: "set_clock", params: {}, serial: "PPTBH01111100001", password: T.B.password });
check("the order is sent with the port, the serial and the password", orders.length === 1 && orders[0].url.endsWith("battery/arm") && orders[0].body.port === 2 && orders[0].body.password === "a person's password" && orders[0].body.serial === "PPTBH01111100001");
check("the password is forgotten", T.B.password === "");
// a viewer sees nothing of it
T.S.session.role = "viewer";
check("a viewer gets no form", page().all("input").length === 0 && page().all("button").length === 0);

console.log(`${checks} checks, ${failed} failed`);
process.exit(failed ? 1 : 0);
