import { ManusClient, type ConnState } from "./client.js";
import { buildFullKeyboardPane } from "./panes/fullkeyboard.js";
import { buildKeyboardPane } from "./panes/keyboard.js";
import { buildShortcutsPane } from "./panes/shortcuts.js";
import { buildTouchpadPane } from "./panes/touchpad.js";
import { Switcher } from "./switcher.js";

const PANES = [
  { id: "keyboard", label: "⌨", name: "テキスト入力" },
  { id: "fullkey", label: "⌗", name: "フルキーボード" },
  { id: "touchpad", label: "🖱", name: "タッチパッド" },
  { id: "shortcuts", label: "★", name: "ショートカット" },
] as const;

const client = new ManusClient();

function el<K extends keyof HTMLElementTagNameMap>(id: string): HTMLElementTagNameMap[K] {
  const node = document.getElementById(id);
  if (!node) throw new Error(`missing #${id}`);
  return node as HTMLElementTagNameMap[K];
}

function setupPairing(onPaired: () => void): void {
  const overlay = el("pairing");
  const input = el<"input">("pin-input");
  const btn = el<"button">("pin-submit");
  const err = el("pin-error");
  overlay.classList.remove("hidden");
  btn.onclick = async () => {
    err.textContent = "";
    const ok = await client.pair(input.value.trim());
    if (ok) {
      overlay.classList.add("hidden");
      onPaired();
    } else {
      err.textContent = "PIN が違います (5 回失敗で 60 秒ロック)";
    }
  };
}

function main(): void {
  const paneHost = el("pane-host");
  const builders: Record<string, () => HTMLElement> = {
    keyboard: () => buildKeyboardPane(client),
    fullkey: () => buildFullKeyboardPane(client),
    touchpad: () => buildTouchpadPane(client),
    shortcuts: () => buildShortcutsPane(client),
  };
  const cache = new Map<string, HTMLElement>();

  function showPane(id: string): void {
    paneHost.replaceChildren();
    let pane = cache.get(id);
    if (!pane) {
      pane = builders[id]!();
      cache.set(id, pane);
    }
    paneHost.append(pane);
    el("pane-name").textContent = PANES.find((p) => p.id === id)!.name;
    for (const item of document.querySelectorAll(".switch-item")) {
      item.classList.toggle("active", (item as HTMLElement).dataset.pane === id);
    }
  }

  const switcher = new Switcher(
    PANES.map((p) => p.id),
    (id) => showPane(id),
  );

  const bar = el("switch-bar");
  for (const p of PANES) {
    const item = document.createElement("button");
    item.className = "switch-item";
    item.dataset.pane = p.id;
    item.textContent = p.label;
    // どのボタンでも「素早く 2 回」で次ペインへ (iOS 地球儀キー相当)。
    item.addEventListener("click", () => switcher.tap());
    bar.append(item);
  }

  const stateDot = el("state-dot");
  client.onState = (s: ConnState) => {
    stateDot.dataset.state = s;
    stateDot.title = s;
    if (s === "unauthorized") setupPairing(() => client.connect());
  };
  client.onLatency = ({ last, avg }) => {
    el("hud-last").textContent = `${last}ms`;
    el("hud-avg").textContent = `${avg}ms`;
  };

  showPane("keyboard");

  if (client.token) client.connect();
  else setupPairing(() => client.connect());
}

main();
