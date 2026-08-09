import { createServer } from "node:http";
import { resolve } from "node:path";
import { install as installVestigium } from "@ludiars/vestigium";
import { WebSocketServer } from "ws";
import { Pairing } from "./auth/pairing.js";
import { loadConfig } from "./config.js";
import { handleApi } from "./http/api.js";
import { serveStatic } from "./http/static-server.js";
import { HelperManager } from "./inject/helper-manager.js";
import { Session } from "./ws/session.js";

const rootDir = resolve(import.meta.dirname, "..");
const config = loadConfig(rootDir);
const publicDir = resolve(rootDir, "public");

const vestigium = installVestigium({ serviceCode: "manus", captureConsole: true });

// 前提の fail-fast: PIN 未設定なら Pairing 構築時に throw する (無認可で公開しない)。
const pairing = new Pairing(rootDir, config.tokenTtlHours);
const helper = new HelperManager(rootDir, config.helperDir);

const httpServer = createServer((req, res) => {
  handleApi(req, res, pairing, () => helper.up)
    .then((handled) => {
      if (!handled) serveStatic(req, res, publicDir);
    })
    .catch((err) => {
      console.error(`[http] ${(err as Error).message}`);
      if (!res.headersSent) res.writeHead(500).end("internal error");
    });
});

const wss = new WebSocketServer({ noServer: true });
httpServer.on("upgrade", (req, socket, head) => {
  const url = new URL(req.url ?? "/", "http://localhost");
  if (url.pathname !== "/ws") {
    socket.destroy();
    return;
  }
  wss.handleUpgrade(req, socket, head, (ws) => {
    if (!Session.authorize(ws, url.searchParams.get("token"), pairing)) return;
    new Session(ws, helper).attach();
  });
});

async function main(): Promise<void> {
  await helper.start();
  httpServer.listen(config.port, config.bind, () => {
    console.log(`[manus] listening on http://${config.bind}:${config.port} (helper up)`);
  });
}

function shutdown(): void {
  console.log("[manus] shutting down");
  helper.stop();
  wss.close();
  httpServer.close();
  void vestigium.shutdown().finally(() => process.exit(0));
}
process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
process.on("uncaughtException", (err) => {
  console.error(`[fatal] uncaughtException: ${err.stack ?? err.message}`);
  helper.stop();
  process.exit(1);
});
process.on("unhandledRejection", (reason) => {
  console.error(`[fatal] unhandledRejection: ${String(reason)}`);
  helper.stop();
  process.exit(1);
});

main().catch((err) => {
  console.error(`[fatal] 起動に失敗しました: ${(err as Error).message}`);
  helper.stop();
  process.exit(1);
});
