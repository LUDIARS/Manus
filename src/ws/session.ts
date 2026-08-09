import type { WebSocket } from "ws";
import type { Pairing } from "../auth/pairing.js";
import type { HelperManager } from "../inject/helper-manager.js";
import { keyCommand, textCommand } from "../inject/keyboard.js";
import { buttonCommand, moveCommand, wheelCommand } from "../inject/mouse.js";
import { ClientMessageSchema, type ClientMessage } from "./protocol.js";

/**
 * 認可済み WS 接続 1 本のメッセージディスパッチ。
 * protocol schema で検証したメッセージだけを helper コマンドに変換して注入する。
 * seq 付きは helper の注入完了後に ack を返す (client 側の遅延計測の基準)。
 */
export class Session {
  constructor(
    private readonly ws: WebSocket,
    private readonly helper: HelperManager,
  ) {}

  static authorize(ws: WebSocket, token: string | null, pairing: Pairing): boolean {
    if (!token || !pairing.verifyToken(token)) {
      ws.close(4401, "unauthorized");
      return false;
    }
    return true;
  }

  attach(): void {
    this.ws.on("message", (data) => void this.onMessage(data.toString()));
  }

  private async onMessage(raw: string): Promise<void> {
    let msg: ClientMessage;
    try {
      msg = ClientMessageSchema.parse(JSON.parse(raw));
    } catch (err) {
      this.sendError("bad_message", (err as Error).message);
      return;
    }
    if (msg.type === "ping") {
      this.ws.send(JSON.stringify({ type: "pong", seq: msg.seq }));
      return;
    }
    try {
      await this.helper.send(this.toCommand(msg));
      if (msg.seq !== undefined) this.ws.send(JSON.stringify({ type: "ack", seq: msg.seq }));
    } catch (err) {
      this.sendError("inject_failed", (err as Error).message, msg.seq);
    }
  }

  private toCommand(msg: Exclude<ClientMessage, { type: "ping" }>) {
    switch (msg.type) {
      case "text":
        return textCommand(msg.text);
      case "key":
        return keyCommand(msg.key, msg.modifiers, msg.action);
      case "mouse.move":
        return moveCommand(msg.dx, msg.dy);
      case "mouse.button":
        return buttonCommand(msg.button, msg.action);
      case "mouse.wheel":
        return wheelCommand(msg.dx ?? 0, msg.dy ?? 0);
    }
  }

  private sendError(code: string, message: string, seq?: number): void {
    this.ws.send(JSON.stringify({ type: "error", code, message, seq }));
  }
}
