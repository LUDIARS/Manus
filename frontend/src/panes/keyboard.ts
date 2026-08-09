import type { ManusClient } from "../client.js";

const SPECIAL_KEYS: Array<{ key: string; label: string }> = [
  { key: "esc", label: "Esc" },
  { key: "tab", label: "Tab" },
  { key: "enter", label: "Enter" },
  { key: "bs", label: "⌫" },
  { key: "del", label: "Del" },
  { key: "left", label: "←" },
  { key: "down", label: "↓" },
  { key: "up", label: "↑" },
  { key: "right", label: "→" },
  { key: "hankaku", label: "半/全" },
  { key: "henkan", label: "変換" },
  { key: "muhenkan", label: "無変換" },
  { key: "kana", label: "かな" },
];

/** キーボードペイン: 確定送信 + ライブ送信 + JIS 特殊キーバー。 */
export function buildKeyboardPane(client: ManusClient): HTMLElement {
  const pane = document.createElement("div");
  pane.className = "pane pane-keyboard";

  const textarea = document.createElement("textarea");
  textarea.className = "kbd-input";
  textarea.placeholder = "ここに入力 (iPhone の日本語 IME で確定して送信)";
  textarea.autocapitalize = "off";
  textarea.autocomplete = "off";

  let live = false;
  let sent = "";

  // ライブ送信: 追記分だけを差分で送る。確定変換で末尾が置換された場合は退避して打ち直す。
  textarea.addEventListener("input", () => {
    if (!live) return;
    const value = textarea.value;
    if (value.startsWith(sent)) {
      const delta = value.slice(sent.length);
      if (delta) client.send({ type: "text", text: delta });
    } else {
      const back = sent.length;
      for (let i = 0; i < back; i++) client.send({ type: "key", key: "bs", action: "press" });
      client.send({ type: "text", text: value });
    }
    sent = value;
  });

  const controls = document.createElement("div");
  controls.className = "kbd-controls";

  const sendBtn = document.createElement("button");
  sendBtn.textContent = "送信";
  sendBtn.className = "btn primary";
  sendBtn.addEventListener("click", () => {
    if (textarea.value) client.send({ type: "text", text: textarea.value });
    textarea.value = "";
    sent = "";
  });

  const enterBtn = document.createElement("button");
  enterBtn.textContent = "送信+Enter";
  enterBtn.className = "btn";
  enterBtn.addEventListener("click", () => {
    if (textarea.value) client.send({ type: "text", text: textarea.value });
    client.send({ type: "key", key: "enter", action: "press" });
    textarea.value = "";
    sent = "";
  });

  const liveLabel = document.createElement("label");
  liveLabel.className = "live-toggle";
  const liveCheck = document.createElement("input");
  liveCheck.type = "checkbox";
  liveCheck.addEventListener("change", () => {
    live = liveCheck.checked;
    sent = live ? textarea.value : "";
  });
  liveLabel.append(liveCheck, document.createTextNode(" ライブ送信"));

  controls.append(sendBtn, enterBtn, liveLabel);

  const special = document.createElement("div");
  special.className = "special-keys";
  for (const { key, label } of SPECIAL_KEYS) {
    const btn = document.createElement("button");
    btn.textContent = label;
    btn.className = "key-btn";
    btn.addEventListener("click", () => client.send({ type: "key", key, action: "press" }));
    special.append(btn);
  }

  pane.append(textarea, controls, special);
  return pane;
}
