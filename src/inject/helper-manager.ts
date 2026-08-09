import { spawn, spawnSync, type ChildProcessWithoutNullStreams } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { createInterface } from "node:readline";
import { resolve } from "node:path";

const CSC_CANDIDATES = [
  "C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe",
  "C:/Windows/Microsoft.NET/Framework/v4.0.30319/csc.exe",
];

export interface HelperCommand {
  op: "ping" | "text" | "key" | "mouse_move" | "mouse_button" | "wheel";
  [field: string]: unknown;
}

interface Pending {
  resolve: () => void;
  reject: (err: Error) => void;
}

/**
 * C# 注入ヘルパの寿命所有者。
 * csc.exe でのビルド (ソース hash 変化時のみ) / spawn / JSON-line RPC / クラッシュ時再起動 /
 * shutdown 時の確実な kill をここに集約する。
 */
export class HelperManager {
  private child: ChildProcessWithoutNullStreams | null = null;
  private readonly pending = new Map<number, Pending>();
  private nextId = 1;
  private stopped = false;
  private restartDelayMs = 500;
  private readonly exePath: string;
  private readonly sourcePath: string;

  constructor(private readonly rootDir: string, helperDir: string) {
    this.sourcePath = resolve(rootDir, "helper", "ManusInputHelper.cs");
    this.exePath = resolve(rootDir, helperDir, "ManusInputHelper.exe");
  }

  get up(): boolean {
    return this.child !== null;
  }

  /** ソース hash が変わっていたら csc.exe で再ビルドする。csc 不在は fail-fast。 */
  build(): void {
    const source = readFileSync(this.sourcePath);
    const hash = createHash("sha256").update(source).digest("hex");
    const hashPath = `${this.exePath}.srchash`;
    if (existsSync(this.exePath) && existsSync(hashPath) && readFileSync(hashPath, "utf8") === hash) {
      return;
    }
    const csc = CSC_CANDIDATES.find((p) => existsSync(p));
    if (!csc) {
      throw new Error("[fatal] csc.exe が見つかりません (.NET Framework 4.x が必要です)");
    }
    mkdirSync(resolve(this.exePath, ".."), { recursive: true });
    const result = spawnSync(
      csc,
      ["/nologo", "/optimize", "/target:exe", `/out:${this.exePath}`, "/r:System.Web.Extensions.dll", this.sourcePath],
      { encoding: "utf8" },
    );
    if (result.status !== 0) {
      throw new Error(`[fatal] ヘルパのビルドに失敗しました:\n${result.stdout}\n${result.stderr}`);
    }
    writeFileSync(hashPath, hash);
    console.log(`[helper] built ${this.exePath}`);
  }

  async start(): Promise<void> {
    this.build();
    await this.spawnChild();
  }

  private spawnChild(): Promise<void> {
    return new Promise((resolvePromise, rejectPromise) => {
      const child = spawn(this.exePath, [], { stdio: ["pipe", "pipe", "pipe"], windowsHide: true });
      let ready = false;
      const rl = createInterface({ input: child.stdout });
      rl.on("line", (line) => {
        let msg: { id?: number; ok?: boolean; error?: string; event?: string };
        try {
          msg = JSON.parse(line);
        } catch {
          console.warn(`[helper] 非 JSON 行を無視: ${line}`);
          return;
        }
        if (msg.event === "ready") {
          ready = true;
          resolvePromise();
          return;
        }
        if (typeof msg.id === "number") {
          const waiter = this.pending.get(msg.id);
          if (waiter) {
            this.pending.delete(msg.id);
            if (msg.ok) waiter.resolve();
            else waiter.reject(new Error(msg.error ?? "helper error"));
          }
        }
      });
      child.stderr.setEncoding("utf8");
      child.stderr.on("data", (chunk: string) => console.warn(`[helper] ${chunk.trimEnd()}`));
      child.on("error", (err) => {
        if (!ready) rejectPromise(err);
        else console.error(`[helper] process error: ${err.message}`);
      });
      child.on("exit", (code, signal) => {
        this.child = null;
        this.failAllPending(new Error(`helper exited (code=${code} signal=${signal})`));
        if (!ready) {
          rejectPromise(new Error(`helper が起動前に終了しました (code=${code})`));
          return;
        }
        if (!this.stopped) {
          console.error(`[helper] crashed (code=${code} signal=${signal}) — ${this.restartDelayMs}ms 後に再起動`);
          setTimeout(() => {
            if (this.stopped) return;
            this.spawnChild().then(
              () => {
                this.restartDelayMs = 500;
              },
              (err) => {
                this.restartDelayMs = Math.min(this.restartDelayMs * 2, 30_000);
                console.error(`[helper] 再起動失敗: ${(err as Error).message}`);
              },
            );
          }, this.restartDelayMs).unref();
        }
      });
      this.child = child;
    });
  }

  private failAllPending(err: Error): void {
    for (const waiter of this.pending.values()) waiter.reject(err);
    this.pending.clear();
  }

  /** 1 コマンド送信 → ヘルパの完了応答まで待つ (この完了が遅延 HUD の ack 基準)。 */
  send(command: HelperCommand): Promise<void> {
    const child = this.child;
    if (!child) return Promise.reject(new Error("helper_down"));
    const id = this.nextId++;
    return new Promise((resolvePromise, rejectPromise) => {
      this.pending.set(id, { resolve: resolvePromise, reject: rejectPromise });
      child.stdin.write(`${JSON.stringify({ id, ...command })}\n`, (err) => {
        if (err) {
          this.pending.delete(id);
          rejectPromise(err);
        }
      });
    });
  }

  stop(): void {
    this.stopped = true;
    if (this.child) {
      this.child.stdin.end(); // stdin close で helper は自然終了する
      const child = this.child;
      setTimeout(() => {
        if (child.exitCode === null) child.kill();
      }, 1000).unref();
      this.child = null;
    }
    this.failAllPending(new Error("helper stopped"));
  }
}
