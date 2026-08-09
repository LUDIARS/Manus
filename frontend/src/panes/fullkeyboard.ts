import type { ManusClient } from "../client.js";
import { JIS108_ROWS, type KeyDef } from "./jis108-layout.js";

type ModName = "ctrl" | "shift" | "alt" | "win";

const ECHO_MS = 1200; // 押したキーが見える時間

/**
 * JIS 日本語 108 キー フルキーボードペイン。
 * 実キーボードの物理配置を 16:9 横向きで固定表示する (画面の縦横に追従して回転・変形しない)。
 * 中央揃え。全キーは KeyCode (VK) 直流し。修飾キーはトグル保持し次キーに合成する。
 * 押したキーは一定時間ハイライト + 画面上部にエコー表示する。
 */
export function buildFullKeyboardPane(client: ManusClient): HTMLElement {
  const pane = document.createElement("div");
  pane.className = "pane pane-fullkey";

  // 押鍵エコー (直近入力を一定時間表示)
  const echo = document.createElement("div");
  echo.className = "fk-echo";

  // 16:9 の固定枠。縦持ちでも横持ちでも同じ向き・同じ比率を保つ。
  const stage = document.createElement("div");
  stage.className = "fk-stage";
  const board = document.createElement("div");
  board.className = "fk-board";

  const held = new Set<ModName>();
  const modButtons = new Map<ModName, HTMLButtonElement[]>();

  function refreshMods(): void {
    for (const [mod, btns] of modButtons) {
      for (const b of btns) b.classList.toggle("held", held.has(mod));
    }
    board.classList.toggle("shifted", held.has("shift"));
  }

  // char/vk を送ったら one-shot の修飾キー (win 以外) は自動解除する実キー感覚。
  function consumeMods(): void {
    let changed = false;
    for (const mod of ["ctrl", "shift", "alt"] as const) {
      if (held.delete(mod)) changed = true;
    }
    if (changed) refreshMods();
  }

  function flashEcho(text: string): void {
    const chip = document.createElement("span");
    chip.className = "fk-echo-chip";
    chip.textContent = text;
    echo.append(chip);
    // 表示 → フェード → 除去
    requestAnimationFrame(() => chip.classList.add("show"));
    setTimeout(() => chip.classList.add("fade"), ECHO_MS);
    setTimeout(() => chip.remove(), ECHO_MS + 300);
    // チップが溜まりすぎないよう上限
    while (echo.childElementCount > 12) echo.firstElementChild?.remove();
  }

  function echoLabel(def: Extract<KeyDef, { kind: "key" }>, mods: ModName[]): string {
    const face = held.has("shift") && def.sub ? def.sub : def.label || "Space";
    const prefix = mods.filter((m) => m !== "shift").map((m) => m[0]!.toUpperCase()).join("+");
    return prefix ? `${prefix}+${face}` : face;
  }

  function pressKey(def: Extract<KeyDef, { kind: "key" }>, btn: HTMLButtonElement): void {
    const mods = [...held];
    flashEcho(echoLabel(def, mods));
    client.send({ type: "key", key: def.vk, modifiers: mods.length ? mods : undefined, action: "press" });
    btn.classList.add("pressed");
    setTimeout(() => btn.classList.remove("pressed"), 140);
    consumeMods();
  }

  function toggleMod(mod: ModName): void {
    if (held.has(mod)) held.delete(mod);
    else held.add(mod);
    refreshMods();
  }

  for (const row of JIS108_ROWS) {
    const rowEl = document.createElement("div");
    rowEl.className = "fk-row";
    for (const def of row) {
      const btn = document.createElement("button");
      btn.className = "fk-key";
      const w = def.w ?? 1;
      btn.style.flexGrow = String(w);
      btn.style.flexBasis = "0";

      if (def.kind === "key") {
        if (def.wide) btn.classList.add("fk-wide");
        renderKeyLabel(btn, def);
        btn.addEventListener("click", () => pressKey(def, btn));
      } else {
        btn.classList.add("fk-mod");
        btn.textContent = def.label;
        const list = modButtons.get(def.mod) ?? [];
        list.push(btn);
        modButtons.set(def.mod, list);
        btn.addEventListener("click", () => toggleMod(def.mod));
      }
      rowEl.append(btn);
    }
    board.append(rowEl);
  }

  stage.append(board);
  pane.append(echo, stage);
  return pane;
}

// キートップ: sub があれば上に小さく、label を下に大きく (実キーボードのキートップ)。
function renderKeyLabel(btn: HTMLButtonElement, def: Extract<KeyDef, { kind: "key" }>): void {
  if (def.sub) {
    const top = document.createElement("span");
    top.className = "fk-sub";
    top.textContent = def.sub;
    const bottom = document.createElement("span");
    bottom.className = "fk-main";
    bottom.textContent = def.label;
    btn.append(top, bottom);
  } else {
    btn.textContent = def.label;
  }
}
