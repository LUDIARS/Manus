# Manus

iPhone のブラウザから、キーボードレスの Windows 機 (ROG Ally X 等) へ
**キーボード / マウス / ショートカット入力** を注入するリモート入力サービス。
iPhone を最大限活用し、インストール不要 (公開 URL を開くだけ) で使う。
サーバは loopback bind で、外部到達は Cloudflare Tunnel 経由 (他の CF 系サービスに合わせる)。

## 仕組み

- Node サーバが LAN に Web UI + WebSocket を公開。
- 打鍵は常駐 C# ヘルパ (`csc.exe` で自動ビルド) の Win32 `SendInput` で注入。
  - 文字列は Unicode 注入 (`KEYEVENTF_UNICODE`) — 日本語 IME 状態・配列に依存せず入る。
  - 特殊キー / ショートカットは JIS 日本語 108 キー Qwerty の仮想キーコード。
- 認可はペアリング PIN → 24h トークン。

現状は **MVP モック** (spec/feature/mvp-mock.md)。入力遅延の実測が目的。
UI は iOS/Android のキーボード切替に倣い、**画面左端のバーを素早く 2 回押す**と
キーボード / タッチパッド / ショートカットを循環切替。上部に遅延 HUD (last/avg)。

後続: 最前面アプリ (exe/URL) 監視によるプロファイル自動切替、カスタム配置ショートカット、
3D マウス的操作、ゲームパッド (ViGEm)。設計は `spec/plan/design.md`。

## 使い方

`spec/setup/setup.md` 参照。要約:

```powershell
npm install
npm run build
npm run set-pin -- 123456      # PIN 必須 (未設定だと起動拒否)
```

起動は Excubitor 経由 (code: `manus`, port 5390, loopback bind)。
iPhone からは Cloudflare Tunnel の公開 URL を開く (ingress 設定は CF ダッシュボード = neco 管理)。
ローカル検証は `http://127.0.0.1:5390/`。
