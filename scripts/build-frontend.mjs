// frontend/src の TS を esbuild で bundle → public/js。SPA フレームワーク不使用。
import { build } from "esbuild";
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");

await build({
  entryPoints: [resolve(root, "frontend/src/main.ts")],
  bundle: true,
  format: "esm",
  target: "es2022",
  outfile: resolve(root, "public/js/main.js"),
  sourcemap: true,
  logLevel: "info",
});
console.log("[build-frontend] done");
