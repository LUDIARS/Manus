// ヘルパの csc ビルド + spawn + ping 応答のみ確認する (キー注入はしない)。
// セッション内で安全に「ヘルパが立つか」を裏取りするためのスモーク。
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { HelperManager } from "../dist/inject/helper-manager.js";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const helper = new HelperManager(root, "data/helper");
await helper.start();
console.log("[smoke] helper started");
await helper.send({ op: "ping" });
console.log("[smoke] ping ok");
helper.stop();
console.log("[smoke] stopped — OK");
process.exit(0);
