import { describe, expect, it } from "vitest";
import { keyCommand, textCommand } from "./keyboard.js";

describe("textCommand", () => {
  it("文字列をそのまま text op にする", () => {
    expect(textCommand("あA1")).toEqual({ op: "text", text: "あA1" });
  });
});

describe("keyCommand", () => {
  it("修飾キーを先頭に、対象キーを末尾に並べる", () => {
    // ctrl=0x11, c=0x43
    expect(keyCommand("c", ["ctrl"])).toEqual({ op: "key", vks: [0x11, 0x43], action: "press" });
  });

  it("複数修飾は指定順で先頭に積む", () => {
    // ctrl=0x11 shift=0x10 s=0x53
    expect(keyCommand("s", ["ctrl", "shift"])).toEqual({ op: "key", vks: [0x11, 0x10, 0x53], action: "press" });
  });

  it("action を透過する", () => {
    expect(keyCommand("enter", [], "down").action).toBe("down");
  });

  it("未知キー名は投げる", () => {
    expect(() => keyCommand("nope")).toThrow(/unknown key/);
  });
});
