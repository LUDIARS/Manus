import type { ManusClient } from "../client.js";

// MVP 用の静的ショートカット (プロファイル機構は後続実装)。
const SHORTCUTS: Array<{ label: string; key: string; modifiers?: string[]; text?: string }> = [
  { label: "コピー", key: "c", modifiers: ["ctrl"] },
  { label: "貼付", key: "v", modifiers: ["ctrl"] },
  { label: "切取", key: "x", modifiers: ["ctrl"] },
  { label: "元に戻す", key: "z", modifiers: ["ctrl"] },
  { label: "やり直し", key: "y", modifiers: ["ctrl"] },
  { label: "全選択", key: "a", modifiers: ["ctrl"] },
  { label: "保存", key: "s", modifiers: ["ctrl"] },
  { label: "検索", key: "f", modifiers: ["ctrl"] },
  { label: "タブ閉", key: "w", modifiers: ["ctrl"] },
  { label: "更新", key: "f5" },
  { label: "Alt+Tab", key: "tab", modifiers: ["alt"] },
  { label: "デスクトップ", key: "d", modifiers: ["win"] },
];

/** ショートカットペイン (MVP は静的モック)。 */
export function buildShortcutsPane(client: ManusClient): HTMLElement {
  const pane = document.createElement("div");
  pane.className = "pane pane-shortcuts";

  const grid = document.createElement("div");
  grid.className = "shortcut-grid";
  for (const sc of SHORTCUTS) {
    const btn = document.createElement("button");
    btn.textContent = sc.label;
    btn.className = "shortcut-btn";
    btn.addEventListener("click", () => {
      if (sc.text) client.send({ type: "text", text: sc.text });
      else client.send({ type: "key", key: sc.key, modifiers: sc.modifiers, action: "press" });
    });
    grid.append(btn);
  }

  const note = document.createElement("p");
  note.className = "note";
  note.textContent = "※ MVP 静的モック。アプリ別プロファイルは後続実装。";

  pane.append(grid, note);
  return pane;
}
