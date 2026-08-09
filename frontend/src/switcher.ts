/**
 * 画面左端の切替バー。iOS/Android のキーボード切替に倣い、
 * 「素早く 2 回押す (ダブルタップ)」でペインを循環させる。
 * 誤爆防止のため単発タップでは切り替えない。
 */
const DOUBLE_TAP_MS = 400;

export class Switcher {
  private lastTap = 0;
  private index = 0;

  constructor(
    private readonly panes: readonly string[],
    private readonly onChange: (paneId: string, index: number) => void,
    private readonly now: () => number = () => performance.now(),
  ) {}

  /** バーのタップ 1 回。DOUBLE_TAP_MS 以内の 2 回目で次ペインへ。 */
  tap(): void {
    const t = this.now();
    if (t - this.lastTap <= DOUBLE_TAP_MS) {
      this.lastTap = 0; // 3 連打が 2 回分に化けないようリセット
      this.advance();
    } else {
      this.lastTap = t;
    }
  }

  private advance(): void {
    this.index = (this.index + 1) % this.panes.length;
    this.onChange(this.panes[this.index]!, this.index);
  }

  get current(): string {
    return this.panes[this.index]!;
  }
}
