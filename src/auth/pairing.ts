import { createHash, createHmac, randomBytes, timingSafeEqual } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";

export interface PairingFiles {
  pairingPath: string;
  secretPath: string;
}

export function pairingFiles(rootDir: string): PairingFiles {
  return {
    pairingPath: resolve(rootDir, "data", "pairing.json"),
    secretPath: resolve(rootDir, "data", "session-secret"),
  };
}

function hashPin(pin: string, salt: string): string {
  return createHash("sha256").update(`${salt}:${pin}`).digest("hex");
}

/** PIN を salt+SHA-256 で保存する (set-pin CLI から)。平文 PIN は保持しない。 */
export function storePin(rootDir: string, pin: string): void {
  if (!/^\d{4,10}$/.test(pin)) {
    throw new Error("PIN は 4〜10 桁の数字で指定してください");
  }
  const { pairingPath } = pairingFiles(rootDir);
  const salt = randomBytes(16).toString("hex");
  mkdirSync(dirname(pairingPath), { recursive: true });
  writeFileSync(pairingPath, JSON.stringify({ salt, pinHash: hashPin(pin, salt) }));
}

export class Pairing {
  private readonly salt: string;
  private readonly pinHash: string;
  private readonly secret: Buffer;
  private readonly ttlMs: number;
  private failCount = 0;
  private lockedUntil = 0;

  /** PIN 未設定は起動時に fail-fast する (無認可で LAN に注入面を開かない)。 */
  constructor(rootDir: string, ttlHours: number, now: () => number = Date.now) {
    this.now = now;
    const { pairingPath, secretPath } = pairingFiles(rootDir);
    if (!existsSync(pairingPath)) {
      throw new Error(`[fatal] ペアリング PIN が未設定です。先に \`npm run set-pin -- <pin>\` を実行してください (${pairingPath})`);
    }
    const stored = JSON.parse(readFileSync(pairingPath, "utf8")) as { salt: string; pinHash: string };
    this.salt = stored.salt;
    this.pinHash = stored.pinHash;
    if (!existsSync(secretPath)) {
      mkdirSync(dirname(secretPath), { recursive: true });
      writeFileSync(secretPath, randomBytes(32));
    }
    this.secret = readFileSync(secretPath);
    this.ttlMs = ttlHours * 3600_000;
  }

  private readonly now: () => number;

  /** PIN 照合。5 連続失敗で 60 秒ロック。成功でトークンを返す。 */
  pair(pin: string): { ok: true; token: string } | { ok: false; error: "locked" | "bad_pin" } {
    if (this.now() < this.lockedUntil) return { ok: false, error: "locked" };
    const attempt = Buffer.from(hashPin(pin, this.salt));
    const expected = Buffer.from(this.pinHash);
    if (attempt.length !== expected.length || !timingSafeEqual(attempt, expected)) {
      this.failCount += 1;
      if (this.failCount >= 5) {
        this.lockedUntil = this.now() + 60_000;
        this.failCount = 0;
      }
      return { ok: false, error: "bad_pin" };
    }
    this.failCount = 0;
    return { ok: true, token: this.issueToken() };
  }

  private issueToken(): string {
    const payload = `${this.now() + this.ttlMs}`;
    const sig = createHmac("sha256", this.secret).update(payload).digest("hex");
    return `${payload}.${sig}`;
  }

  verifyToken(token: string): boolean {
    const dot = token.indexOf(".");
    if (dot <= 0) return false;
    const payload = token.slice(0, dot);
    const sig = token.slice(dot + 1);
    const expected = createHmac("sha256", this.secret).update(payload).digest("hex");
    const sigBuf = Buffer.from(sig);
    const expBuf = Buffer.from(expected);
    if (sigBuf.length !== expBuf.length || !timingSafeEqual(sigBuf, expBuf)) return false;
    return this.now() < Number(payload);
  }
}
