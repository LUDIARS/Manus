/**
 * Realforce R3 テンキーレス 日本語配列 (JIS TKL, 91 キー) の物理レイアウト定義。
 * 参考: 東プレ REALFORCE R3S R3SC11 系 (日本語配列テンキーレス)。
 *
 * TKL は「メインブロック」と右側の「ナビゲーションブロック」が横に並ぶ。
 * この定義は行ごとに main[] と nav[] を持ち、描画側が 2 ブロックを間隔を空けて配置する。
 *
 * 送信方式は「KeyCode 直流し」に統一 (neco 指示 2026-08-09):
 *  - すべてのキーは vk (jis-vk-map のキー名) を送る (type:key)。文字も Unicode ではなく
 *    物理キーの VK を送るため、Shift 面や IME 状態は Windows 側 + 修飾トグルで決まる。
 *  - kind "key": 通常キー。label=キートップ主表示、sub=Shift 面表示 (任意)。
 *  - kind "mod": 修飾キー (ctrl/shift/alt/win)。押下でトグル保持し次キーに合成。
 *  - kind "gap": ブロック内の空き (F 行の視覚的グルーピング用)。
 */
export type KeyDef =
  | { kind: "key"; vk: string; label: string; sub?: string; w?: number; wide?: boolean }
  | { kind: "mod"; mod: "ctrl" | "shift" | "alt" | "win"; label: string; w?: number; side?: "l" | "r" }
  | { kind: "gap"; w: number };

/** 1 段 = メインブロックのキー列 + ナビブロックのキー列 (無い段は nav 空)。 */
export interface BoardRow {
  main: KeyDef[];
  nav: KeyDef[];
}

const gap = (w: number): KeyDef => ({ kind: "gap", w });

// メインブロックは 15U 幅基準。ナビブロックは 3U 幅基準 (main と nav の間に 0.5U の間隔)。
export const JIS_TKL_ROWS: BoardRow[] = [
  // F 行: Esc | F1-F4 | F5-F8 | F9-F12 || PrtSc ScrLk Pause
  {
    main: [
      { kind: "key", vk: "esc", label: "Esc" },
      gap(0.5),
      { kind: "key", vk: "f1", label: "F1" },
      { kind: "key", vk: "f2", label: "F2" },
      { kind: "key", vk: "f3", label: "F3" },
      { kind: "key", vk: "f4", label: "F4" },
      gap(0.5),
      { kind: "key", vk: "f5", label: "F5" },
      { kind: "key", vk: "f6", label: "F6" },
      { kind: "key", vk: "f7", label: "F7" },
      { kind: "key", vk: "f8", label: "F8" },
      gap(0.5),
      { kind: "key", vk: "f9", label: "F9" },
      { kind: "key", vk: "f10", label: "F10" },
      { kind: "key", vk: "f11", label: "F11" },
      { kind: "key", vk: "f12", label: "F12" },
    ],
    nav: [
      { kind: "key", vk: "printscreen", label: "PrtSc" },
      { kind: "key", vk: "scrolllock", label: "ScrLk" },
      { kind: "key", vk: "pause", label: "Pause" },
    ],
  },
  // 数字段: 半角/全角 1..0 - ^ ¥ BS || Insert Home PageUp
  {
    main: [
      { kind: "key", vk: "hankaku", label: "半/全" },
      { kind: "key", vk: "1", label: "1", sub: "!" },
      { kind: "key", vk: "2", label: "2", sub: '"' },
      { kind: "key", vk: "3", label: "3", sub: "#" },
      { kind: "key", vk: "4", label: "4", sub: "$" },
      { kind: "key", vk: "5", label: "5", sub: "%" },
      { kind: "key", vk: "6", label: "6", sub: "&" },
      { kind: "key", vk: "7", label: "7", sub: "'" },
      { kind: "key", vk: "8", label: "8", sub: "(" },
      { kind: "key", vk: "9", label: "9", sub: ")" },
      { kind: "key", vk: "0", label: "0", sub: "~" },
      { kind: "key", vk: "minus", label: "-", sub: "=" },
      { kind: "key", vk: "caret", label: "^", sub: "~" },
      { kind: "key", vk: "yen", label: "¥", sub: "|" },
      { kind: "key", vk: "bs", label: "⌫", w: 1 },
    ],
    nav: [
      { kind: "key", vk: "ins", label: "Ins" },
      { kind: "key", vk: "home", label: "Home" },
      { kind: "key", vk: "pgup", label: "PgUp" },
    ],
  },
  // Tab QWERTY @[ Enter(上) || Delete End PageDown
  {
    main: [
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
    nav: [
      { kind: "key", vk: "del", label: "Del" },
      { kind: "key", vk: "end", label: "End" },
      { kind: "key", vk: "pgdn", label: "PgDn" },
    ],
  },
  // 英数 ASDF ;: ]} Enter(下) || (nav 空)
  {
    main: [
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
    nav: [],
  },
  // 左Shift ZXCV ,./ \_(ろ) 右Shift || (上矢印)
  {
    main: [
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
    nav: [
      gap(1),
      { kind: "key", vk: "up", label: "↑" },
      gap(1),
    ],
  },
  // Ctrl Win Alt 無変換 Space 変換 かな Alt App Ctrl || 左下右 矢印
  {
    main: [
      { kind: "mod", mod: "ctrl", label: "Ctrl", w: 1.25, side: "l" },
      { kind: "mod", mod: "win", label: "Win", w: 1.25, side: "l" },
      { kind: "mod", mod: "alt", label: "Alt", w: 1.25, side: "l" },
      { kind: "key", vk: "muhenkan", label: "無変換", w: 1.25 },
      { kind: "key", vk: "space", label: "", w: 3.5 },
      { kind: "key", vk: "henkan", label: "変換", w: 1.25 },
      { kind: "key", vk: "kana", label: "かな", w: 1.25 },
      { kind: "mod", mod: "alt", label: "Alt", w: 1.25, side: "r" },
      { kind: "key", vk: "apps", label: "▤", w: 1.25 },
      { kind: "mod", mod: "ctrl", label: "Ctrl", w: 1.25, side: "r" },
    ],
    nav: [
      { kind: "key", vk: "left", label: "←" },
      { kind: "key", vk: "down", label: "↓" },
      { kind: "key", vk: "right", label: "→" },
    ],
  },
];
