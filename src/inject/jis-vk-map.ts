/**
 * JIS 日本語 108 キー Qwerty 配列のキー名 → Windows 仮想キーコード (VK)。
 * key メッセージ / ショートカット定義の語彙の正本。
 * 文字入力は Unicode 注入 (keyboard.ts) を使うため、ここは特殊キー・ショートカット用。
 */
export const MODIFIER_VK = {
  ctrl: 0x11,
  shift: 0x10,
  alt: 0x12,
  win: 0x5b,
} as const;

export type ModifierName = keyof typeof MODIFIER_VK;

export const JIS_VK_MAP: Readonly<Record<string, number>> = {
  // 制御
  enter: 0x0d,
  esc: 0x1b,
  tab: 0x09,
  space: 0x20,
  bs: 0x08,
  del: 0x2e,
  ins: 0x2d,
  // ナビゲーション
  up: 0x26,
  down: 0x28,
  left: 0x25,
  right: 0x27,
  home: 0x24,
  end: 0x23,
  pgup: 0x21,
  pgdn: 0x22,
  // ファンクション
  f1: 0x70, f2: 0x71, f3: 0x72, f4: 0x73, f5: 0x74, f6: 0x75,
  f7: 0x76, f8: 0x77, f9: 0x78, f10: 0x79, f11: 0x7a, f12: 0x7b,
  // JIS 固有 (日本語 108 キー)
  hankaku: 0xf3, // 半角/全角 (VK_OEM_AUTO。DBE toggle は 0xF3/0xF4 ペアだが押下は 0xF3 で切替る)
  henkan: 0x1c, // 変換 (VK_CONVERT)
  muhenkan: 0x1d, // 無変換 (VK_NONCONVERT)
  kana: 0x15, // カタカナ/ひらがな (VK_KANA)
  eisu: 0x14, // 英数 (VK_CAPITAL = CapsLock/英数)
  // 英数字 (VK は ASCII 一致)
  ...Object.fromEntries("abcdefghijklmnopqrstuvwxyz".split("").map((c) => [c, c.toUpperCase().charCodeAt(0)])),
  ...Object.fromEntries("0123456789".split("").map((c) => [c, c.charCodeAt(0)])),
  // JIS 記号キー (物理キー等価。Shift 面は modifiers で表現する)
  minus: 0xbd, // -=
  caret: 0xde, // ^~ (JIS: VK_OEM_7)
  yen: 0xdc, // ¥| (VK_OEM_5)
  at: 0xc0, // @` (JIS: VK_OEM_3)
  lbracket: 0xdb, // [{
  rbracket: 0xdd, // ]}
  semicolon: 0xbb, // ;+ (JIS: VK_OEM_PLUS)
  colon: 0xba, // :* (JIS: VK_OEM_1)
  comma: 0xbc, // ,<
  period: 0xbe, // .>
  slash: 0xbf, // /?
  backslash: 0xe2, // \_ (JIS ろ、VK_OEM_102)
  // テンキー
  num0: 0x60, num1: 0x61, num2: 0x62, num3: 0x63, num4: 0x64,
  num5: 0x65, num6: 0x66, num7: 0x67, num8: 0x68, num9: 0x69,
  nummul: 0x6a, numadd: 0x6b, numsub: 0x6d, numdec: 0x6e, numdiv: 0x6f,
  numlock: 0x90,
  // その他
  capslock: 0x14,
  printscreen: 0x2c,
  scrolllock: 0x91,
  pause: 0x13,
  apps: 0x5d, // メニューキー
};

export function vkOf(key: string): number {
  const vk = JIS_VK_MAP[key];
  if (vk === undefined) throw new Error(`unknown key name: ${key}`);
  return vk;
}
