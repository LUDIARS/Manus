import type { ManusClient } from "../client.js";
import { JIS_TKL_ROWS, type BoardRow, type KeyDef } from "./jis108-layout.js";

type ModName = "ctrl" | "shift" | "alt" | "win";

const ECHO_MS = 1200; // 押したキーが見える時間

// 盤面の U 座標系。実キーボード比率で absolute 配置する (完全再現)。
const MAIN_U = 15; // メインブロックの横 U
const NAV_U = 3; // ナビブロックの横 U
const BLOCK_GAP_U = 0.6; // メイン↔ナビ間隔
const BOARD_W = MAIN_U + BLOCK_GAP_U + NAV_U; // 総横 U
const ROWS = 6; // F行/数字/Tab/ASDF/Shift/最下段
const NAV_X0 = MAIN_U + BLOCK_GAP_U; // ナビブロックの左端 U

// 各キーの絶対配置 (U 単位)。描画時に % へ変換する。
interface PlacedKey {
  def: Extract<KeyDef, { kind: "key" }> | Extract<KeyDef, { kind: "mod" }>;
  x: number;
  y: number;
  w: number;
  h: number;
}

/** JIS_TKL_ROWS を絶対座標へ展開する。Enter は逆L字 (上腕+下部) の 2 矩形として同じ vk で置く。 */
function placeKeys(): PlacedKey[] {
  const placed: PlacedKey[] = [];
  JIS_TKL_ROWS.forEach((row: BoardRow, rowIndex) => {
    // メインブロック: 左から U を積む
    let x = 0;
    for (const def of row.main) {
      const w = def.w ?? 1;
      if (def.kind === "gap") {
        x += w;
        continue;
      }
      // Tab 段(rowIndex=2)の Enter は上腕 (高さ1)。ASDF 段(rowIndex=3)には Enter を置かず、
      // ここで下部 (幅広・高さ1) を一緒に置いて逆L字にする。
      if (def.kind === "key" && def.vk === "enter" && rowIndex === 2) {
        const armW = 1.5; // 上腕
        const lowerW = 2.25; // 下部 (ASDF 段の右端、広い)
        placed.push({ def, x, y: rowIndex, w: armW, h: 1 }); // 上腕
        placed.push({ def, x: MAIN_U - lowerW, y: rowIndex + 1, w: lowerW, h: 1 }); // 下部
        x += armW;
        continue;
      }
      // ASDF 段(rowIndex=3)の Enter は上段の逆L字下部で描画済みなのでスキップ。
      if (def.kind === "key" && def.vk === "enter" && rowIndex === 3) continue;
      placed.push({ def, x, y: rowIndex, w, h: 1 });
      x += w;
    }
    // ナビブロック
    let nx = NAV_X0;
    for (const def of row.nav) {
      const w = def.w ?? 1;
      if (def.kind === "gap") {
        nx += w;
        continue;
      }
      placed.push({ def, x: nx, y: rowIndex, w, h: 1 });
      nx += w;
    }
  });
  return placed;
}

/**
 * Realforce R3 テンキーレス 日本語配列のフルキーボードペイン。
 * 各キーを U 座標で absolute 配置し、逆L字 Enter まで物理配列を完全再現する。
 * 盤面は 19:10 の枠に内接。縦持ち時は 90 度回転 (CSS)。全キー KeyCode(VK) 直流し。
 * 修飾キーはトグル保持し次キーに合成。押鍵は一定時間ハイライト + 上部エコー。
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
  board.style.setProperty("--board-w", String(BOARD_W));
  board.style.setProperty("--board-rows", String(ROWS));

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

  function pressKey(def: Extract<KeyDef, { kind: "key" }>, els: HTMLButtonElement[]): void {
    const mods = [...held];
    flashEcho(echoLabel(def, mods));
    client.send({ type: "key", key: def.vk, modifiers: mods.length ? mods : undefined, action: "press" });
    for (const e of els) e.classList.add("pressed");
    setTimeout(() => els.forEach((e) => e.classList.remove("pressed")), 140);
    consumeMods();
  }

  function toggleMod(mod: ModName): void {
    if (held.has(mod)) held.delete(mod);
    else held.add(mod);
    refreshMods();
  }

  // 逆L字 Enter は 2 要素で同一キー。押下ハイライトを両方に効かせるため vk 単位で束ねる。
  const enterEls: HTMLButtonElement[] = [];

  for (const pk of placeKeys()) {
    const btn = document.createElement("button");
    btn.className = "fk-key";
    // U 座標 → 盤面比率 (%)。left/top/width/height を CSS 変数経由で算出。
    btn.style.left = `calc(${pk.x} / var(--board-w) * 100%)`;
    btn.style.top = `calc(${pk.y} / var(--board-rows) * 100%)`;
    btn.style.width = `calc(${pk.w} / var(--board-w) * 100%)`;
    btn.style.height = `calc(${pk.h} / var(--board-rows) * 100%)`;

    if (pk.def.kind === "key") {
      const def = pk.def;
      renderKeyLabel(btn, def);
      if (def.vk === "enter") {
        enterEls.push(btn);
        btn.addEventListener("click", () => pressKey(def, enterEls));
      } else {
        btn.addEventListener("click", () => pressKey(def, [btn]));
      }
    } else {
      const def = pk.def;
      btn.classList.add("fk-mod");
      btn.textContent = def.label;
      const list = modButtons.get(def.mod) ?? [];
      list.push(btn);
      modButtons.set(def.mod, list);
      btn.addEventListener("click", () => toggleMod(def.mod));
    }
    board.append(btn);
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
