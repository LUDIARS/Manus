import type { IncomingMessage, ServerResponse } from "node:http";
import type { Pairing } from "../auth/pairing.js";

function readBody(req: IncomingMessage): Promise<string> {
  return new Promise((resolvePromise, rejectPromise) => {
    let body = "";
    req.on("data", (chunk) => {
      body += chunk;
      if (body.length > 4096) rejectPromise(new Error("body too large"));
    });
    req.on("end", () => resolvePromise(body));
    req.on("error", rejectPromise);
  });
}

function json(res: ServerResponse, status: number, payload: unknown): void {
  const body = JSON.stringify(payload);
  res.writeHead(status, { "content-type": "application/json; charset=utf-8" });
  res.end(body);
}

/** JSON API を処理したら true を返す (未処理なら false で静的配信へ委ねる)。 */
export async function handleApi(
  req: IncomingMessage,
  res: ServerResponse,
  pairing: Pairing,
  helperUp: () => boolean,
): Promise<boolean> {
  const url = new URL(req.url ?? "/", "http://localhost");

  if (url.pathname === "/health") {
    json(res, 200, { ok: true, service: "manus", helper: helperUp() ? "up" : "down" });
    return true;
  }

  if (url.pathname === "/api/pair" && req.method === "POST") {
    let pin: unknown;
    try {
      pin = JSON.parse(await readBody(req)).pin;
    } catch {
      json(res, 400, { error: "bad_request" });
      return true;
    }
    if (typeof pin !== "string") {
      json(res, 400, { error: "bad_request" });
      return true;
    }
    const result = pairing.pair(pin);
    if (result.ok) json(res, 200, { token: result.token });
    else json(res, result.error === "locked" ? 429 : 401, { error: result.error });
    return true;
  }

  return false;
}
