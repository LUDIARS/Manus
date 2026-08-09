import { describe, expect, it } from "vitest";
import { JIS_VK_MAP, vkOf } from "./jis-vk-map.js";

describe("JIS_VK_MAP", () => {
  it("英字は ASCII 大文字の VK", () => {
    expect(vkOf("a")).toBe(0x41);
    expect(vkOf("z")).toBe(0x5a);
  });

  it("矢印・ファンクション・JIS 固有キーを持つ", () => {
    expect(vkOf("up")).toBe(0x26);
    expect(vkOf("f12")).toBe(0x7b);
    expect(vkOf("henkan")).toBe(0x1c);
    expect(vkOf("muhenkan")).toBe(0x1d);
    expect(vkOf("kana")).toBe(0x15);
  });

  it("VK が重複しない (テンキーと本体キーは別コード)", () => {
    const seen = new Map<number, string>();
    const dups: string[] = [];
    for (const [name, vk] of Object.entries(JIS_VK_MAP)) {
      const prev = seen.get(vk);
      // capslock と eisu は同一 VK(0x14) を共有するのは仕様なので除外
      if (prev && !(new Set([prev, name]).size === 2 && vk === 0x14)) dups.push(`${prev}/${name}=${vk}`);
      seen.set(vk, name);
    }
    expect(dups).toEqual([]);
  });

  it("未知キーは投げる", () => {
    expect(() => vkOf("banana")).toThrow();
  });
});
