import type { ManusClient } from "../client.js";

const MOVE_SCALE = 1.6; // 指移動 px → カーソル px
const TAP_MAX_MS = 200;
const TAP_MAX_MOVE = 8;
const SCROLL_SCALE = 0.6;

/** タッチパッドペイン: 1 本指=移動 / タップ=左クリック / 2 本指=スクロール + ボタン。 */
export function buildTouchpadPane(client: ManusClient): HTMLElement {
  const pane = document.createElement("div");
  pane.className = "pane pane-touchpad";

  const surface = document.createElement("div");
  surface.className = "touch-surface";
  surface.textContent = "ここをなぞる";

  let lastX = 0;
  let lastY = 0;
  let startTime = 0;
  let moved = 0;
  let twoFinger = false;
  let lastScrollY = 0;

  surface.addEventListener(
    "touchstart",
    (ev) => {
      ev.preventDefault();
      const t = ev.touches[0]!;
      lastX = t.clientX;
      lastY = t.clientY;
      startTime = performance.now();
      moved = 0;
      twoFinger = ev.touches.length >= 2;
      lastScrollY = t.clientY;
    },
    { passive: false },
  );

  surface.addEventListener(
    "touchmove",
    (ev) => {
      ev.preventDefault();
      const t = ev.touches[0]!;
      if (ev.touches.length >= 2) {
        twoFinger = true;
        const dy = t.clientY - lastScrollY;
        if (Math.abs(dy) >= 2) {
          client.send({ type: "mouse.wheel", dy: Math.round(-dy * SCROLL_SCALE) * 4 });
          lastScrollY = t.clientY;
        }
        return;
      }
      const dx = t.clientX - lastX;
      const dy = t.clientY - lastY;
      moved += Math.abs(dx) + Math.abs(dy);
      lastX = t.clientX;
      lastY = t.clientY;
      client.send({ type: "mouse.move", dx: Math.round(dx * MOVE_SCALE), dy: Math.round(dy * MOVE_SCALE) });
    },
    { passive: false },
  );

  surface.addEventListener("touchend", (ev) => {
    ev.preventDefault();
    const dur = performance.now() - startTime;
    if (!twoFinger && dur <= TAP_MAX_MS && moved <= TAP_MAX_MOVE) {
      client.send({ type: "mouse.button", button: "left", action: "click" });
    }
  });

  const buttons = document.createElement("div");
  buttons.className = "touch-buttons";
  for (const [button, label] of [
    ["left", "左クリック"],
    ["middle", "中クリック"],
    ["right", "右クリック"],
  ] as const) {
    const btn = document.createElement("button");
    btn.textContent = label;
    btn.className = "btn";
    btn.addEventListener("click", () => client.send({ type: "mouse.button", button, action: "click" }));
    buttons.append(btn);
  }

  pane.append(surface, buttons);
  return pane;
}
