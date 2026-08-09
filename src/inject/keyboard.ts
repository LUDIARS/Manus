import type { HelperCommand } from "./helper-manager.js";
import { MODIFIER_VK, vkOf, type ModifierName } from "./jis-vk-map.js";

/** 文字列を Unicode 注入コマンドにする (改行/タブは helper 側で Enter/Tab に変換)。 */
export function textCommand(text: string): HelperCommand {
  return { op: "text", text };
}

/**
 * キー + 修飾キーを VK シーケンスにする。
 * press: modifier down → key down → key up → modifier up (逆順) の 1 コマンドで helper が発行。
 * modifier の押し順・離し順を helper 側に委ねず、ここで vks 配列の順序として確定させる。
 */
export function keyCommand(
  key: string,
  modifiers: ModifierName[] = [],
  action: "press" | "down" | "up" = "press",
): HelperCommand {
  const modVks = modifiers.map((m) => MODIFIER_VK[m]);
  const keyVk = vkOf(key);
  // helper は vks を「先頭→末尾で down、末尾→先頭で up」する。modifier を先に置く。
  return { op: "key", vks: [...modVks, keyVk], action };
}
