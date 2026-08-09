import { resolve } from "node:path";
import { storePin } from "../auth/pairing.js";

const pin = process.argv[2];
if (!pin) {
  console.error("使い方: npm run set-pin -- <4〜10桁の数字>");
  process.exit(1);
}
const rootDir = resolve(import.meta.dirname, "..", "..");
storePin(rootDir, pin);
console.log("ペアリング PIN を設定しました。");
