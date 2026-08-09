import type { ManusClient } from "../client.js";
import { JIS_TKL_ROWS, type BoardRow, type KeyDef } from "./jis108-layout.js";

type ModName = "ctrl" | "shift" | "alt" | "win";

const ECHO_MS = 1200; // 押したキーが見える時間

// メインブロックとナビブロックの幅比 (U 合計)。段によらず固定枠にするため定数で持つ。
const MAIN_U = 15;
const NAV_U = 3;
const BLOCK_GAP_U = 0.6;

/**
 * Realforce R3 テンキーレス 日本語配列 (JIS TKL) のフルキーボードペイン。
 * 実キーボードの物理配置を 16:9 横向きで固定表示する (画面の縦横に追従して回転・変形しない)。
 * 中央揃え。全キーは KeyCode (VK) 直流し。修飾キーはトグル保持し次キーに合成する。
 * 押したキーは一定時間ハイライト + 画面上部にエコー表示する。
 */
export function buildFullKeyboardPane(client: ManusClient): HTMLElement {
  const pane = document.createElement("div");
  pane.className = "pane pane-fullkey";

  const echo = document.createElement("div");
  echo.className = "fk-echo";

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
    requestAnimationFrame(() => chip.classList.add("show"));
    setTimeout(() => chip.classList.add("fade"), ECHO_MS);
    setTimeout(() => chip.remove(), ECHO_MS + 300);
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

  function renderKey(def: KeyDef): HTMLElement {
    const w = def.w ?? 1;
    if (def.kind === "gap") {
      const spacer = document.createElement("div");
      spacer.className = "fk-gap";
      spacer.style.flexGrow = String(w);
      spacer.style.flexBasis = "0";
      return spacer;
    }
    const btn = document.createElement("button");
    btn.className = "fk-key";
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
    return btn;
  }

  function sumU(keys: KeyDef[]): number {
    return keys.reduce((acc, k) => acc + (k.w ?? 1), 0);
  }

  for (const row of JIS_TKL_ROWS as BoardRow[]) {
    const rowEl = document.createElement("div");
    rowEl.className = "fk-row";

    // メインブロック (固定 MAIN_U 幅。段が MAIN_U 未満なら右に空きを足して幅を揃える)
    const mainEl = document.createElement("div");
    mainEl.className = "fk-block fk-main-block";
    mainEl.style.flexGrow = String(MAIN_U);
    for (const def of row.main) mainEl.append(renderKey(def));
    const mainPad = MAIN_U - sumU(row.main);
    if (mainPad > 0.01) mainEl.append(renderKey({ kind: "gap", w: mainPad }));

    // ブロック間の間隔
    const blockGap = document.createElement("div");
    blockGap.className = "fk-block-gap";
    blockGap.style.flexGrow = String(BLOCK_GAP_U);

    // ナビブロック (固定 NAV_U 幅。空段でも枠を確保して桁を揃える)
    const navEl = document.createElement("div");
    navEl.className = "fk-block fk-nav-block";
    navEl.style.flexGrow = String(NAV_U);
    for (const def of row.nav) navEl.append(renderKey(def));
    const navPad = NAV_U - sumU(row.nav);
    if (navPad > 0.01) navEl.append(renderKey({ kind: "gap", w: navPad }));

    rowEl.append(mainEl, blockGap, navEl);
    board.append(rowEl);
  }

  stage.append(board);
  pane.append(echo, stage);
  return pane;
}

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
