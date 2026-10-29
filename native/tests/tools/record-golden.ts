// Records golden input/output pairs from the Node implementation (src/) for the
// C++ compatibility tests (native/tests/golden_tests.cpp).
//
//   npm ci --ignore-scripts          # dev dependencies (tsx, ws, zod)
//   npx tsx native/tests/tools/record-golden.ts
//
// Writes native/tests/fixtures/golden/{vk_map,session,pairing,http,ws}.json.
// Nothing here touches a real service or injects input: the C# helper is replaced
// by a fake that records the commands it would have received, the HTTP server
// binds 127.0.0.1 on an ephemeral port, and the pairing files live in a temp dir.
import { createHash } from "node:crypto";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { createServer, request as httpRequest, type IncomingHttpHeaders } from "node:http";
import { connect } from "node:net";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { WebSocketServer } from "ws";
import { Pairing, pairingFiles, storePin } from "../../../src/auth/pairing.js";
import { handleApi } from "../../../src/http/api.js";
import { serveStatic } from "../../../src/http/static-server.js";
import type { HelperCommand, HelperManager } from "../../../src/inject/helper-manager.js";
import { JIS_VK_MAP, MODIFIER_VK } from "../../../src/inject/jis-vk-map.js";
import { Session } from "../../../src/ws/session.js";

const here = dirname(fileURLToPath(import.meta.url));
const fixtures = resolve(here, "..", "fixtures");
const outDir = join(fixtures, "golden");
const publicDir = join(fixtures, "http-root", "public");
mkdirSync(outDir, { recursive: true });

const NOTE =
  "Node 実装 (src/) から native/tests/tools/record-golden.ts で生成。テスト専用の値 " +
  "(PIN 123456、署名鍵はバイト列 0x00..0x1f) で、実データではない。";

function write(name: string, value: Record<string, unknown>): void {
  writeFileSync(join(outDir, name), `${JSON.stringify({ _note: NOTE, ...value }, null, 2)}\n`);
  console.log(`[golden] ${name}`);
}

// ---------------------------------------------------------------- vk_map.json
write("vk_map.json", { JIS_VK_MAP, MODIFIER_VK });

// --------------------------------------------------------------- session.json
class FakeHelper {
  commands: string[] = [];
  down = false;
  get up(): boolean {
    return !this.down;
  }
  send(command: HelperCommand): Promise<void> {
    if (this.down) return Promise.reject(new Error("helper_down"));
    this.commands.push(JSON.stringify(command));
    return Promise.resolve();
  }
}

class FakeSocket {
  sent: string[] = [];
  closed: { code: number; reason: string } | null = null;
  send(data: string): void {
    this.sent.push(data);
  }
  on(): void {}
  close(code: number, reason: string): void {
    this.closed = { code, reason };
  }
}

const sessionInputs: string[] = [
  // valid messages of every type
  '{"type":"ping"}',
  '{"type":"ping","seq":1}',
  '{"type":"ping","seq":1.0}',
  '{"type":"ping","seq":-0}',
  '{"type":"ping","seq":0.5}',
  '{"type":"ping","seq":1e21}',
  '{"type":"ping","seq":1.5e-7}',
  '{"type":"ping","seq":12345678901234567890}',
  '{"type":"text","text":"hello","seq":2}',
  '{"type":"text","text":"日本語\\n改行\\tタブ\\r","seq":3}',
  '{"type":"text","text":"😀","seq":4}',
  '{"type":"text","text":""}',
  '{"type":"text","text":"x"}',
  '{"type":"key","key":"enter","seq":5}',
  '{"type":"key","key":"c","modifiers":["ctrl"],"seq":6}',
  '{"type":"key","key":"s","modifiers":["ctrl","shift"],"action":"press","seq":7}',
  '{"type":"key","key":"a","modifiers":["shift","shift"],"action":"down"}',
  '{"type":"key","key":"a","action":"up","seq":8}',
  '{"type":"key","key":"henkan","seq":9}',
  '{"type":"key","key":"f12","modifiers":["win","alt"],"seq":10}',
  '{"type":"key","key":"num0","modifiers":[],"seq":11}',
  '{"type":"mouse.move","dx":10,"dy":-5,"seq":12}',
  '{"type":"mouse.move","dx":1.9,"dy":-1.9}',
  '{"type":"mouse.move","dx":-0.5,"dy":0.5,"seq":13}',
  '{"type":"mouse.button","button":"left","action":"click","seq":14}',
  '{"type":"mouse.button","button":"right","action":"down"}',
  '{"type":"mouse.button","button":"middle","action":"up","seq":15}',
  '{"type":"mouse.wheel","dy":-120,"seq":16}',
  '{"type":"mouse.wheel","dx":30.7}',
  '{"type":"mouse.wheel","seq":17}',
  // extra fields are stripped, duplicate keys keep the last value
  '{"type":"key","key":"a","extra":{"deep":[1,2,3]},"seq":18}',
  '{"type":"ping","type":"text","text":"dup","seq":19}',
  // unknown key names
  '{"type":"key","key":"banana","seq":20}',
  '{"type":"key","key":"Enter"}',
  // invalid messages (bad_message, no seq in the reply)
  "",
  "not json",
  "123",
  "null",
  "[]",
  '"text"',
  "{}",
  '{"type":"pad.button","seq":21}',
  '{"type":"shortcut","profileId":"p","buttonId":"b"}',
  '{"type":"text","seq":22}',
  '{"type":"text","text":1}',
  '{"type":"text","text":"a","seq":"1"}',
  '{"type":"text","text":"a","seq":null}',
  '{"type":"key","key":""}',
  `{"type":"key","key":"${"k".repeat(17)}"}`,
  '{"type":"key","key":"a","modifiers":["meta"]}',
  '{"type":"key","key":"a","modifiers":"ctrl"}',
  '{"type":"key","key":"a","action":"click"}',
  '{"type":"mouse.move","dx":1}',
  '{"type":"mouse.move","dx":"1","dy":1}',
  '{"type":"mouse.button","button":"x1","action":"click"}',
  '{"type":"mouse.button","button":"left","action":"press"}',
  '{"type":"mouse.wheel","dy":null}',
  `{"type":"text","text":"${"a".repeat(4096)}","seq":23}`,
  `{"type":"text","text":"${"a".repeat(4097)}","seq":24}`,
  `{"type":"text","text":"${"😀".repeat(2048)}","seq":25}`,
  `{"type":"text","text":"${"😀".repeat(2048)}a","seq":26}`,
  // cases where the C++ implementation intentionally differs (see golden_tests.cpp)
  '{"type":"key","key":"constructor","seq":27}',
  '{"type":"key","key":"__proto__"}',
  '{"type":"text","text":"\\ud800","seq":28}',
  '{"type":"mouse.move","dx":3000000000,"dy":0,"seq":29}',
];

async function recordSession(): Promise<void> {
  const cases = [];
  for (const raw of sessionInputs) {
    const helper = new FakeHelper();
    const socket = new FakeSocket();
    const session = new Session(socket as never, helper as unknown as HelperManager);
    await (session as unknown as { onMessage(raw: string): Promise<void> }).onMessage(raw);
    cases.push({ raw, reply: socket.sent[0] ?? null, command: helper.commands[0] ?? null });
  }
  // helper down: the command is never delivered
  const helper = new FakeHelper();
  helper.down = true;
  const socket = new FakeSocket();
  const session = new Session(socket as never, helper as unknown as HelperManager);
  const downRaw = '{"type":"mouse.move","dx":1,"dy":1,"seq":30}';
  await (session as unknown as { onMessage(raw: string): Promise<void> }).onMessage(downRaw);
  write("session.json", { cases, helperDown: { raw: downRaw, reply: socket.sent[0] ?? null } });
}

// --------------------------------------------------------------- pairing.json
// Deterministic secret (bytes 0x00..0x1f) and clock; the salt comes from storePin.
const secret = Buffer.from(Array.from({ length: 32 }, (_, i) => i));

function freshRoot(): string {
  return mkdtempSync(join(tmpdir(), "manus-golden-"));
}

function prepareRoot(pin: string): string {
  const root = freshRoot();
  storePin(root, pin);
  writeFileSync(pairingFiles(root).secretPath, secret);
  return root;
}

function recordPairing(): void {
  const root = prepareRoot("123456");
  const stored = JSON.parse(readFileSync(pairingFiles(root).pairingPath, "utf8")) as { salt: string; pinHash: string };
  let now = 1_700_000_000_000;
  const pairing = new Pairing(root, 24, () => now);
  const issued = pairing.pair("123456");
  if (!issued.ok) throw new Error("pairing failed");
  const mac = issued.token.slice(issued.token.indexOf(".") + 1);
  const verify = [
    { token: issued.token, now: now, ok: false },
    { token: issued.token, now: now + 24 * 3_600_000 - 1, ok: false },
    { token: issued.token, now: now + 24 * 3_600_000, ok: false },
    { token: `1700000000000.${mac}`, now, ok: false },
    { token: `${issued.token}x`, now, ok: false },
    { token: issued.token.toUpperCase(), now, ok: false },
    { token: `.${mac}`, now, ok: false },
    { token: "garbage", now, ok: false },
    { token: "", now, ok: false },
  ];
  for (const v of verify) {
    now = v.now;
    v.ok = pairing.verifyToken(v.token);
  }
  // lock-out sequence on a fresh instance
  const lockPairing = new Pairing(root, 24, () => now);
  const attempts: { at: number; pin: string; result: unknown }[] = [];
  const script: [number, string][] = [
    [0, "000000"], [0, "000000"], [0, "000000"], [0, "000000"], [0, "123456"],
    [0, "1"], [0, "2"], [0, "3"], [0, "4"], [0, "5"], [0, "123456"], [59_999, "123456"], [60_000, "000000"],
    [60_000, "123456"],
  ];
  const base = 1_800_000_000_000;
  for (const [offset, pin] of script) {
    now = base + offset;
    const r = lockPairing.pair(pin);
    attempts.push({ at: now, pin, result: r.ok ? { ok: true } : r });
  }
  write("pairing.json", {
    pin: "123456",
    salt: stored.salt,
    pinHash: stored.pinHash,
    hashCheck: createHash("sha256").update(`${stored.salt}:123456`).digest("hex"),
    secretHex: secret.toString("hex"),
    ttlHours: 24,
    issuedAt: 1_700_000_000_000,
    token: issued.token,
    verify,
    lockSequence: attempts,
  });
  rmSync(root, { recursive: true, force: true });
}

// ------------------------------------------------------------ http.json / ws.json
interface HttpCase {
  name: string;
  method: string;
  path: string;
  headers?: Record<string, string>;
  body?: string;
  ifNoneMatchFrom?: string;
}

interface RawCase {
  name: string;
  raw: string;
}

const httpCases: HttpCase[] = [
  { name: "health", method: "GET", path: "/health" },
  { name: "health_query", method: "GET", path: "/health?x=1" },
  { name: "health_post", method: "POST", path: "/health", body: "" },
  { name: "root", method: "GET", path: "/" },
  { name: "spa_keyboard", method: "GET", path: "/keyboard" },
  { name: "spa_dir", method: "GET", path: "/js" },
  { name: "css", method: "GET", path: "/style.css" },
  { name: "css_revalidate", method: "GET", path: "/style.css", ifNoneMatchFrom: "css" },
  { name: "css_other_etag", method: "GET", path: "/style.css", headers: { "If-None-Match": '"nope"' } },
  { name: "js", method: "GET", path: "/js/main.js" },
  { name: "sourcemap", method: "GET", path: "/js/main.js.map" },
  { name: "svg", method: "GET", path: "/icon.svg" },
  { name: "query_ignored", method: "GET", path: "/style.css?v=abc" },
  { name: "missing", method: "GET", path: "/missing.js" },
  { name: "dotdot_resolved", method: "GET", path: "/../style.css" },
  { name: "encoded_dotdot_resolved", method: "GET", path: "/%2e%2e/style.css" },
  { name: "encoded_slash_inside", method: "GET", path: "/js/..%2fstyle.css" },
  { name: "traversal", method: "GET", path: "/..%2fsecret.txt" },
  { name: "traversal_deep", method: "GET", path: "/js/..%2f..%2fsecret.txt" },
  { name: "sibling_prefix", method: "GET", path: "/..%2fpublic-evil/x.js" },
  { name: "malformed_uri", method: "GET", path: "/%E0%A4%A" },
  { name: "invalid_utf8_uri", method: "GET", path: "/%FF.js" },
  { name: "trailing_slash_file", method: "GET", path: "/style.css/" },
  { name: "dotfile_is_spa", method: "GET", path: "/.env" },
  { name: "post_root_is_static", method: "POST", path: "/", body: "" },
  { name: "get_pair_is_static", method: "GET", path: "/api/pair" },
  { name: "pair_empty", method: "POST", path: "/api/pair", body: "" },
  { name: "pair_not_json", method: "POST", path: "/api/pair", body: "not json" },
  { name: "pair_array", method: "POST", path: "/api/pair", body: "[]" },
  { name: "pair_null", method: "POST", path: "/api/pair", body: "null" },
  { name: "pair_no_pin", method: "POST", path: "/api/pair", body: "{}" },
  { name: "pair_number_pin", method: "POST", path: "/api/pair", body: '{"pin":123456}' },
  { name: "pair_too_large", method: "POST", path: "/api/pair", body: `{"pin":"${"1".repeat(4090)}"}` },
  { name: "pair_ok", method: "POST", path: "/api/pair", body: '{"pin":"123456"}' },
  { name: "pair_ok_extra_field", method: "POST", path: "/api/pair", body: '{"pin":"123456","x":1}' },
  { name: "pair_exact_limit_bad_pin", method: "POST", path: "/api/pair", body: `{"pin":"${"1".repeat(4086)}"}` },
  { name: "pair_bad_2", method: "POST", path: "/api/pair", body: '{"pin":"000000"}' },
  { name: "pair_bad_3", method: "POST", path: "/api/pair", body: '{"pin":"000000"}' },
  { name: "pair_bad_4", method: "POST", path: "/api/pair", body: '{"pin":"000000"}' },
  { name: "pair_bad_5_locks", method: "POST", path: "/api/pair", body: '{"pin":"000000"}' },
  { name: "pair_locked", method: "POST", path: "/api/pair", body: '{"pin":"123456"}' },
];

const rawCases: RawCase[] = [
  { name: "malformed_request_line", raw: "GARBAGE\r\n\r\n" },
  {
    name: "header_too_large",
    raw: `GET /health HTTP/1.1\r\nHost: x\r\nX-Pad: ${"a".repeat(17 * 1024)}\r\nConnection: close\r\n\r\n`,
  },
  {
    name: "upgrade_other_path",
    raw:
      "GET /other HTTP/1.1\r\nHost: x\r\nConnection: Upgrade\r\nUpgrade: websocket\r\n" +
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n",
  },
  {
    name: "upgrade_health_path",
    raw:
      "GET /health HTTP/1.1\r\nHost: x\r\nConnection: Upgrade\r\nUpgrade: websocket\r\n" +
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n",
  },
  {
    name: "upgrade_post",
    raw:
      "POST /ws HTTP/1.1\r\nHost: x\r\nConnection: Upgrade\r\nUpgrade: websocket\r\nContent-Length: 0\r\n" +
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n",
  },
  {
    name: "upgrade_bad_key",
    raw:
      "GET /ws HTTP/1.1\r\nHost: x\r\nConnection: Upgrade\r\nUpgrade: websocket\r\n" +
      "Sec-WebSocket-Key: short\r\nSec-WebSocket-Version: 13\r\n\r\n",
  },
  {
    name: "upgrade_bad_version",
    raw:
      "GET /ws HTTP/1.1\r\nHost: x\r\nConnection: Upgrade\r\nUpgrade: websocket\r\n" +
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 12\r\n\r\n",
  },
];

function startNodeServer(root: string): Promise<{ port: number; close: () => Promise<void> }> {
  const pairing = new Pairing(root, 24);
  const helper = new FakeHelper();
  const httpServer = createServer((req, res) => {
    handleApi(req, res, pairing, () => helper.up)
      .then((handled) => {
        if (!handled) serveStatic(req, res, publicDir);
      })
      .catch(() => {
        if (!res.headersSent) res.writeHead(500).end("internal error");
      });
  });
  // Same upgrade wiring as src/server.ts.
  const wss = new WebSocketServer({ noServer: true });
  httpServer.on("upgrade", (req, socket, head) => {
    const url = new URL(req.url ?? "/", "http://localhost");
    if (url.pathname !== "/ws") {
      socket.destroy();
      return;
    }
    wss.handleUpgrade(req, socket, head, (ws) => {
      if (!Session.authorize(ws, url.searchParams.get("token"), pairing)) return;
      new Session(ws, helper as unknown as HelperManager).attach();
    });
  });
  return new Promise((resolvePromise) => {
    httpServer.listen(0, "127.0.0.1", () => {
      const address = httpServer.address();
      const port = typeof address === "object" && address ? address.port : 0;
      resolvePromise({
        port,
        close: () =>
          new Promise((done) => {
            for (const client of wss.clients) client.terminate();
            httpServer.closeAllConnections();
            httpServer.close(() => done());
          }),
      });
    });
  });
}

function pickHeaders(headers: IncomingHttpHeaders): Record<string, string | null> {
  const one = (name: string): string | null => {
    const v = headers[name];
    return v === undefined ? null : String(v);
  };
  return { "content-type": one("content-type"), "cache-control": one("cache-control"), etag: one("etag") };
}

function sendHttp(port: number, c: HttpCase, etags: Map<string, string>): Promise<Record<string, unknown>> {
  return new Promise((resolvePromise, rejectPromise) => {
    const headers: Record<string, string> = { Connection: "close", ...(c.headers ?? {}) };
    if (c.body !== undefined) {
      headers["Content-Type"] = "application/json";
      headers["Content-Length"] = String(Buffer.byteLength(c.body));
    }
    if (c.ifNoneMatchFrom) headers["If-None-Match"] = etags.get(c.ifNoneMatchFrom) ?? "";
    const req = httpRequest({ host: "127.0.0.1", port, method: c.method, path: c.path, headers }, (res) => {
      const chunks: Buffer[] = [];
      res.on("data", (chunk: Buffer) => chunks.push(chunk));
      res.on("end", () => {
        const body = Buffer.concat(chunks).toString("utf8");
        const picked = pickHeaders(res.headers);
        if (picked.etag) etags.set(c.name, picked.etag);
        resolvePromise({ status: res.statusCode, headers: { ...picked, etag: picked.etag ? "<present>" : null }, body });
      });
    });
    req.on("error", rejectPromise);
    if (c.body !== undefined) req.write(c.body);
    req.end();
  });
}

function sendRaw(port: number, raw: string): Promise<string> {
  return new Promise((resolvePromise) => {
    const socket = connect(port, "127.0.0.1", () => socket.write(raw));
    const chunks: Buffer[] = [];
    socket.on("data", (chunk: Buffer) => chunks.push(chunk));
    const finish = () => resolvePromise(Buffer.concat(chunks).toString("latin1"));
    socket.on("close", finish);
    socket.on("error", () => {});
    setTimeout(() => socket.destroy(), 2000);
  });
}

function rawSummary(response: string): Record<string, unknown> {
  if (response.length === 0) return { closedWithoutResponse: true };
  const statusLine = response.slice(0, response.indexOf("\r\n"));
  return { closedWithoutResponse: false, status: Number(statusLine.split(" ")[1]) };
}

interface WsOutcome {
  opened: boolean;
  closeCode: number | null;
  replies: string[];
}

function runWs(port: number, target: string, messages: string[]): Promise<WsOutcome> {
  return new Promise((resolvePromise) => {
    const outcome: WsOutcome = { opened: false, closeCode: null, replies: [] };
    const ws = new WebSocket(`ws://127.0.0.1:${port}${target}`);
    ws.onopen = () => {
      outcome.opened = true;
      for (const m of messages) ws.send(m);
      if (messages.length > 0) setTimeout(() => ws.close(1000), 300);
    };
    ws.onmessage = (ev) => outcome.replies.push(String(ev.data));
    ws.onclose = (ev) => {
      outcome.closeCode = ev.code;
      resolvePromise(outcome);
    };
    ws.onerror = () => {};
  });
}

async function recordHttpAndWs(): Promise<void> {
  const root = prepareRoot("123456");
  const server = await startNodeServer(root);
  const etags = new Map<string, string>();
  const http: Record<string, unknown>[] = [];
  for (const c of httpCases) {
    const result = await sendHttp(server.port, c, etags);
    if (c.name.startsWith("pair_ok")) result.body = (result.body as string).replace(/"token":"\d+\.[0-9a-f]{64}"/, '"token":"<token>"');
    http.push({ ...c, response: result });
  }
  const raw: Record<string, unknown>[] = [];
  for (const c of rawCases) raw.push({ ...c, response: rawSummary(await sendRaw(server.port, c.raw)) });
  write("http.json", { pin: "123456", cases: http, rawCases: raw });
  await server.close();

  // ws.json on a fresh server (the HTTP run above ends locked out)
  const wsServer = await startNodeServer(root);
  const token = await new Promise<string>((resolvePromise, rejectPromise) => {
    const req = httpRequest(
      { host: "127.0.0.1", port: wsServer.port, method: "POST", path: "/api/pair", headers: { "Content-Type": "application/json" } },
      (res) => {
        const chunks: Buffer[] = [];
        res.on("data", (chunk: Buffer) => chunks.push(chunk));
        res.on("end", () => resolvePromise((JSON.parse(Buffer.concat(chunks).toString("utf8")) as { token: string }).token));
      },
    );
    req.on("error", rejectPromise);
    req.end('{"pin":"123456"}');
  });
  const tampered = token.slice(0, -1) + (token.endsWith("0") ? "1" : "0");
  const wsCases = [
    { name: "no_token", target: "/ws", messages: [] as string[] },
    { name: "empty_token", target: "/ws?token=", messages: [] },
    { name: "garbage_token", target: "/ws?token=garbage", messages: [] },
    { name: "tampered_token", target: "/ws?token=<tampered>", messages: [] },
    {
      name: "valid_token",
      target: "/ws?token=<token>",
      messages: ['{"type":"ping","seq":1}', '{"type":"key","key":"c","modifiers":["ctrl"],"seq":2}', "not json", '{"type":"text","text":"x"}', '{"type":"ping"}'],
    },
    { name: "valid_token_encoded", target: "/ws?a=1&token=<token-encoded>", messages: ['{"type":"ping","seq":3}'] },
  ];
  const ws: Record<string, unknown>[] = [];
  for (const c of wsCases) {
    const target = c.target
      .replace("<tampered>", encodeURIComponent(tampered))
      .replace("<token-encoded>", token.replace(".", "%2E"))
      .replace("<token>", encodeURIComponent(token));
    ws.push({ ...c, outcome: await runWs(wsServer.port, target, c.messages) });
  }
  write("ws.json", { pin: "123456", cases: ws });
  await wsServer.close();
  rmSync(root, { recursive: true, force: true });
}

await recordSession();
recordPairing();
await recordHttpAndWs();
console.log("[golden] done");
