/**
 * JIS 日本語 108 キーボードの物理レイアウト定義。
 * 実キーボードの見た目・キー幅・段構成を忠実に再現する (16:9 横向き固定表示用)。
 *
 * 送信方式は「KeyCode 直流し」に統一 (neco 指示 2026-08-09):
 *  - すべてのキーは vk (jis-vk-map のキー名) を送る (type:key)。文字も Unicode ではなく
 *    物理キーの VK を送るため、Shift 面や IME 状態は Windows 側 + 修飾トグルで決まる。
 *  - kind "key": 通常キー。label=キートップ主表示、sub=Shift 面表示 (任意)。
 *  - kind "mod": 修飾キー (ctrl/shift/alt/win)。押下でトグル保持し次キーに合成。
 */
export type KeyDef =
  | { kind: "key"; vk: string; label: string; sub?: string; w?: number; wide?: boolean }
  | { kind: "mod"; mod: "ctrl" | "shift" | "alt" | "win"; label: string; w?: number; side?: "l" | "r" };

export type Row = KeyDef[];

// w は「キー幅の U 単位」。省略時 1U。段内の合計 U で正規化して 16:9 枠に収める。
export const JIS108_ROWS: Row[] = [
  // 最上段: 半角/全角 + 数字列 + BS
  [
    { kind: "key", vk: "hankaku", label: "半角/全角", w: 1 },
    { kind: "key", vk: "1", label: "1", sub: "!" },
    { kind: "key", vk: "2", label: "2", sub: '"' },
    { kind: "key", vk: "3", label: "3", sub: "#" },
    { kind: "key", vk: "4", label: "4", sub: "$" },
    { kind: "key", vk: "5", label: "5", sub: "%" },
    { kind: "key", vk: "6", label: "6", sub: "&" },
    { kind: "key", vk: "7", label: "7", sub: "'" },
    { kind: "key", vk: "8", label: "8", sub: "(" },
    { kind: "key", vk: "9", label: "9", sub: ")" },
    { kind: "key", vk: "0", label: "0" },
    { kind: "key", vk: "minus", label: "-", sub: "=" },
    { kind: "key", vk: "caret", label: "^", sub: "~" },
    { kind: "key", vk: "yen", label: "¥", sub: "|" },
    { kind: "key", vk: "bs", label: "⌫", w: 1 },
  ],
  // Tab + QWERTY 上段 + @[ + Enter(上半分)
  [
    { kind: "key", vk: "tab", label: "Tab", w: 1.5 },
    { kind: "key", vk: "q", label: "Q" },
    { kind: "key", vk: "w", label: "W" },
    { kind: "key", vk: "e", label: "E" },
    { kind: "key", vk: "r", label: "R" },
    { kind: "key", vk: "t", label: "T" },
    { kind: "key", vk: "y", label: "Y" },
    { kind: "key", vk: "u", label: "U" },
    { kind: "key", vk: "i", label: "I" },
    { kind: "key", vk: "o", label: "O" },
    { kind: "key", vk: "p", label: "P" },
    { kind: "key", vk: "at", label: "@", sub: "`" },
    { kind: "key", vk: "lbracket", label: "[", sub: "{" },
    { kind: "key", vk: "enter", label: "Enter", w: 1.5, wide: true },
  ],
  // 英数(CapsLock) + ASDF 段 + ;: ]} (Enter の左に食い込む)
  [
    { kind: "key", vk: "eisu", label: "英数", w: 1.75 },
    { kind: "key", vk: "a", label: "A" },
    { kind: "key", vk: "s", label: "S" },
    { kind: "key", vk: "d", label: "D" },
    { kind: "key", vk: "f", label: "F" },
    { kind: "key", vk: "g", label: "G" },
    { kind: "key", vk: "h", label: "H" },
    { kind: "key", vk: "j", label: "J" },
    { kind: "key", vk: "k", label: "K" },
    { kind: "key", vk: "l", label: "L" },
    { kind: "key", vk: "semicolon", label: ";", sub: "+" },
    { kind: "key", vk: "colon", label: ":", sub: "*" },
    { kind: "key", vk: "rbracket", label: "]", sub: "}" },
    { kind: "key", vk: "enter", label: "⏎", w: 1.25 },
  ],
  // 左Shift + ZXCV 段 + ,. / \_(ろ) + 右Shift
  [
    { kind: "mod", mod: "shift", label: "Shift", w: 2.25, side: "l" },
    { kind: "key", vk: "z", label: "Z" },
    { kind: "key", vk: "x", label: "X" },
    { kind: "key", vk: "c", label: "C" },
    { kind: "key", vk: "v", label: "V" },
    { kind: "key", vk: "b", label: "B" },
    { kind: "key", vk: "n", label: "N" },
    { kind: "key", vk: "m", label: "M" },
    { kind: "key", vk: "comma", label: ",", sub: "<" },
    { kind: "key", vk: "period", label: ".", sub: ">" },
    { kind: "key", vk: "slash", label: "/", sub: "?" },
    { kind: "key", vk: "backslash", label: "\\", sub: "_" },
    { kind: "mod", mod: "shift", label: "Shift", w: 1.75, side: "r" },
  ],
  // 最下段: Ctrl Win Alt 無変換 Space 変換 かな Alt Ctrl 矢印
  [
    { kind: "mod", mod: "ctrl", label: "Ctrl", w: 1.5, side: "l" },
    { kind: "mod", mod: "win", label: "Win", w: 1.25, side: "l" },
    { kind: "mod", mod: "alt", label: "Alt", w: 1.25, side: "l" },
    { kind: "key", vk: "muhenkan", label: "無変換", w: 1.25 },
    { kind: "key", vk: "space", label: "", w: 4 },
    { kind: "key", vk: "henkan", label: "変換", w: 1.25 },
    { kind: "key", vk: "kana", label: "かな", w: 1.25 },
    { kind: "mod", mod: "alt", label: "Alt", w: 1, side: "r" },
    { kind: "mod", mod: "ctrl", label: "Ctrl", w: 1.25, side: "r" },
    { kind: "key", vk: "left", label: "←", w: 1 },
    { kind: "key", vk: "up", label: "↑", w: 1 },
    { kind: "key", vk: "down", label: "↓", w: 1 },
    { kind: "key", vk: "right", label: "→", w: 1 },
  ],
];
