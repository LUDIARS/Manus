// frontend の TS を esbuild で bundle → public/js。
// style.css を public へコピーし、index.html テンプレートの %HASH% を content hash で
// 差し替えて public/index.html を生成する (Cloudflare エッジキャッシュ対策のキャッシュバスト)。
import { build } from "esbuild";
import { createHash } from "node:crypto";
import { copyFileSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const publicDir = resolve(root, "public");
mkdirSync(resolve(publicDir, "js"), { recursive: true });

await build({
  entryPoints: [resolve(root, "frontend/src/main.ts")],
  bundle: true,
  format: "esm",
  target: "es2022",
  outfile: resolve(publicDir, "js/main.js"),
  sourcemap: true,
  logLevel: "info",
});

// style.css をソース(frontend)から public へ配置
const cssSrc = resolve(root, "frontend/style.css");
const cssOut = resolve(publicDir, "style.css");
copyFileSync(cssSrc, cssOut);

// 生成物 (main.js + style.css) の内容ハッシュ。どちらかが変わればクエリが変わり、
// CF/ブラウザは新 URL として取り直す。
const hash = createHash("sha256")
  .update(readFileSync(resolve(publicDir, "js/main.js")))
  .update(readFileSync(cssOut))
  .digest("hex")
  .slice(0, 12);

const template = readFileSync(resolve(root, "frontend/index.html"), "utf8");
writeFileSync(resolve(publicDir, "index.html"), template.replaceAll("%HASH%", hash));

console.log(`[build-frontend] done (asset hash ${hash})`);
