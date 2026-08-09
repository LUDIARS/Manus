import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { describe, expect, it } from "vitest";
import { Pairing, storePin } from "./pairing.js";

function freshRoot(): string {
  return mkdtempSync(join(tmpdir(), "manus-pair-"));
}

describe("Pairing", () => {
  it("PIN 未設定なら fail-fast する", () => {
    expect(() => new Pairing(freshRoot(), 24)).toThrow(/未設定/);
  });

  it("正しい PIN でトークンを発行し検証が通る", () => {
    const root = freshRoot();
    storePin(root, "123456");
    const p = new Pairing(root, 24);
    const result = p.pair("123456");
    expect(result.ok).toBe(true);
    if (result.ok) expect(p.verifyToken(result.token)).toBe(true);
  });

  it("誤った PIN は拒否し、5 回でロックする", () => {
    const root = freshRoot();
    storePin(root, "123456");
    const p = new Pairing(root, 24);
    for (let i = 0; i < 5; i++) expect(p.pair("000000").ok).toBe(false);
    expect(p.pair("123456")).toEqual({ ok: false, error: "locked" });
  });

  it("改竄トークンを拒否する", () => {
    const root = freshRoot();
    storePin(root, "123456");
    const p = new Pairing(root, 24);
    const r = p.pair("123456");
    if (!r.ok) throw new Error("should pair");
    expect(p.verifyToken(r.token.replace(/.$/, "0"))).toBe(false);
    expect(p.verifyToken("garbage")).toBe(false);
  });

  it("期限切れトークンを拒否する", () => {
    const root = freshRoot();
    storePin(root, "123456");
    let now = 1_000_000;
    const p = new Pairing(root, 1, () => now);
    const r = p.pair("123456");
    if (!r.ok) throw new Error("should pair");
    now += 3600_000 + 1;
    expect(p.verifyToken(r.token)).toBe(false);
  });
});
