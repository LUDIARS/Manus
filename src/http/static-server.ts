import { createReadStream, existsSync, statSync } from "node:fs";
import type { IncomingMessage, ServerResponse } from "node:http";
import { extname, join, normalize, resolve } from "node:path";

const MIME: Record<string, string> = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".svg": "image/svg+xml",
  ".png": "image/png",
  ".webmanifest": "application/manifest+json",
};

/** public/ 配下の静的配信。ルート系パスは index.html を返す (単一ページ SPA 風)。 */
export function serveStatic(req: IncomingMessage, res: ServerResponse, publicDir: string): void {
  const url = new URL(req.url ?? "/", "http://localhost");
  let pathname = decodeURIComponent(url.pathname);
  if (pathname === "/" || !extname(pathname)) pathname = "/index.html";

  const publicRoot = resolve(publicDir);
  const filePath = normalize(join(publicRoot, pathname));
  if (!filePath.startsWith(publicRoot)) {
    res.writeHead(403).end("forbidden");
    return;
  }
  if (!existsSync(filePath) || !statSync(filePath).isFile()) {
    res.writeHead(404).end("not found");
    return;
  }
  res.writeHead(200, { "content-type": MIME[extname(filePath)] ?? "application/octet-stream" });
  createReadStream(filePath).pipe(res);
}
