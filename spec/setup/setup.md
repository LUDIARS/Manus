# Manus セットアップ

## 前提

- Windows (SendInput / csc.exe を使うため)。.NET Framework 4.x (Windows 標準同梱)。
- Node.js >= 22。

## 手順

```powershell
cd <workspace-root>\Manus
npm install            # NODE_ENV=production の shell では --include=dev を付ける
npm run build          # tsc + frontend bundle
npm run set-pin -- 123456   # ペアリング PIN 設定 (必須。未設定だと起動拒否)
```

起動は Excubitor 経由 (`excubitor.catalog.yaml` 配置済み、code: `manus`)。
開発時のみ `npm run dev`。

## 疎通 (Cloudflare Tunnel)

サーバは loopback (127.0.0.1:5390) のみで listen する。外部到達は cloudflared の
ingress (`<sub>.ai-run-do.com → http://localhost:5390`) 経由。ingress 設定は
CF ダッシュボード (neco 管理) が正本で、ローカルからは変更不可。
疎通確認は `http://127.0.0.1:20241/config` の ingress 一覧に Manus のエントリがあるか、で行う。

## iPhone 側

1. Safari で公開 URL (`https://<sub>.ai-run-do.com/`) を開く (LAN 外でも Tunnel 経由で届く)。
2. PIN を入力してペアリング (トークンは localStorage に 24h 保持)。
3. 共有シート → 「ホーム画面に追加」で全画面化 (任意)。

> ローカル検証のみなら `http://127.0.0.1:5390/` を PC のブラウザで開ける。

## ゲームパッド (後回し・未実装)

実装時は ViGEmBus ドライバ + `vigemclient` が必要になる予定。現段階では不要。
