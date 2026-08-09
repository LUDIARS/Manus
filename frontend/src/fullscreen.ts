/**
 * フルスクリーン切替。Fullscreen API が使える環境 (iPadOS Safari / Android / PC) で全画面化する。
 * iPhone Safari は要素フルスクリーン非対応のため、その旨をボタン状態で示す (ホーム画面追加を促す)。
 */
interface FsElement extends HTMLElement {
  webkitRequestFullscreen?: () => Promise<void> | void;
}
interface FsDocument extends Document {
  webkitFullscreenElement?: Element | null;
  webkitExitFullscreen?: () => Promise<void> | void;
}

function fsElement(): Element | null {
  const d = document as FsDocument;
  return d.fullscreenElement ?? d.webkitFullscreenElement ?? null;
}

function supported(): boolean {
  const el = document.documentElement as FsElement;
  return typeof el.requestFullscreen === "function" || typeof el.webkitRequestFullscreen === "function";
}

async function enter(): Promise<void> {
  const el = document.documentElement as FsElement;
  if (el.requestFullscreen) await el.requestFullscreen();
  else if (el.webkitRequestFullscreen) await el.webkitRequestFullscreen();
}

async function exit(): Promise<void> {
  const d = document as FsDocument;
  if (d.exitFullscreen) await d.exitFullscreen();
  else if (d.webkitExitFullscreen) await d.webkitExitFullscreen();
}

export function setupFullscreen(button: HTMLButtonElement): void {
  if (!supported()) {
    // iPhone Safari 等: 非対応。ボタンは残し、押下でホーム画面追加を案内する。
    button.classList.add("unsupported");
    button.title = "この端末は全画面非対応 (ホーム画面に追加すると全画面になります)";
    button.addEventListener("click", () => {
      button.classList.add("hint");
      setTimeout(() => button.classList.remove("hint"), 2500);
    });
    return;
  }
  const sync = (): void => {
    const active = fsElement() !== null;
    button.classList.toggle("active", active);
    button.textContent = active ? "🗗" : "⛶";
  };
  button.addEventListener("click", () => {
    void (fsElement() ? exit() : enter());
  });
  document.addEventListener("fullscreenchange", sync);
  document.addEventListener("webkitfullscreenchange", sync as EventListener);
  sync();
}
