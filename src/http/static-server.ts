import { createReadStream, statSync } from "node:fs";
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
  let stat;
  try {
    stat = statSync(filePath);
  } catch {
    res.writeHead(404).end("not found");
    return;
  }
  if (!stat.isFile()) {
    res.writeHead(404).end("not found");
    return;
  }

  // no-store = ブラウザもエッジ (Cloudflare) もキャッシュしない。開発中に iPhone や CF の
  // エッジキャッシュで古い bundle を掴む事故を防ぐ (CF は既定で js/css をキャッシュするため
  // no-cache では max-age に上書きされる。no-store は原則キャッシュされない)。
  // ETag も併記し、ブラウザが検証リクエストを出せば 304 で本文を省ける。
  const etag = `"${stat.mtimeMs.toString(36)}-${stat.size.toString(36)}"`;
  const cacheControl = "no-store, no-cache, must-revalidate";
  if (req.headers["if-none-match"] === etag) {
    res.writeHead(304, { etag, "cache-control": cacheControl }).end();
    return;
  }
  res.writeHead(200, {
    "content-type": MIME[extname(filePath)] ?? "application/octet-stream",
    "cache-control": cacheControl,
    etag,
  });
  createReadStream(filePath).pipe(res);
}
