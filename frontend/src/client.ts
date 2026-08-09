/** サーバとの WS/HTTP 通信 + 遅延計測。UI 層から入力送信を隠蔽する。 */
export type ConnState = "disconnected" | "connecting" | "connected" | "unauthorized";

export interface LatencySample {
  last: number;
  avg: number;
}

type OutMessage =
  | { type: "text"; text: string; seq?: number }
  | { type: "key"; key: string; modifiers?: string[]; action?: string; seq?: number }
  | { type: "mouse.move"; dx: number; dy: number; seq?: number }
  | { type: "mouse.button"; button: string; action: string; seq?: number }
  | { type: "mouse.wheel"; dx?: number; dy?: number; seq?: number };

const TOKEN_KEY = "manus.token";

export class ManusClient {
  private ws: WebSocket | null = null;
  private seq = 1;
  private readonly pendingSince = new Map<number, number>();
  private avg = 0;
  private state: ConnState = "disconnected";

  onState: (s: ConnState) => void = () => {};
  onLatency: (s: LatencySample) => void = () => {};

  get token(): string | null {
    return localStorage.getItem(TOKEN_KEY);
  }

  async pair(pin: string): Promise<boolean> {
    const res = await fetch("/api/pair", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ pin }),
    });
    if (!res.ok) return false;
    const { token } = (await res.json()) as { token: string };
    localStorage.setItem(TOKEN_KEY, token);
    return true;
  }

  connect(): void {
    const token = this.token;
    if (!token) {
      this.setState("unauthorized");
      return;
    }
    this.setState("connecting");
    const proto = location.protocol === "https:" ? "wss" : "ws";
    const ws = new WebSocket(`${proto}://${location.host}/ws?token=${encodeURIComponent(token)}`);
    this.ws = ws;
    ws.onopen = () => this.setState("connected");
    ws.onclose = (ev) => {
      this.ws = null;
      if (ev.code === 4401) {
        localStorage.removeItem(TOKEN_KEY);
        this.setState("unauthorized");
      } else {
        this.setState("disconnected");
        setTimeout(() => this.connect(), 1000);
      }
    };
    ws.onmessage = (ev) => this.onServer(JSON.parse(ev.data));
  }

  private onServer(msg: { type: string; seq?: number }): void {
    if ((msg.type === "ack" || msg.type === "pong") && typeof msg.seq === "number") {
      const started = this.pendingSince.get(msg.seq);
      if (started !== undefined) {
        this.pendingSince.delete(msg.seq);
        const rtt = performance.now() - started;
        this.avg = this.avg === 0 ? rtt : this.avg * 0.8 + rtt * 0.2;
        this.onLatency({ last: Math.round(rtt), avg: Math.round(this.avg) });
      }
    }
  }

  /** seq を付けて送信し、ack までの RTT を測る。 */
  send(msg: OutMessage): void {
    if (this.ws?.readyState !== WebSocket.OPEN) return;
    const seq = this.seq++;
    this.pendingSince.set(seq, performance.now());
    this.ws.send(JSON.stringify({ ...msg, seq }));
  }

  private setState(s: ConnState): void {
    this.state = s;
    this.onState(s);
  }

  get connState(): ConnState {
    return this.state;
  }
}
