# Manus 設計書

iPhone のブラウザで入力した文字列・タッチ操作・ショートカット操作を、キーボードレスの
Windows デバイス (ROG Ally X 等) に **OS レベルの入力** として注入するリモート入力サービス。
PC 側の最前面アプリを監視し、**アプリ (exe 名 / ブラウザ URL) に応じたショートカット UI**
を iPhone 側に自動で出す。

## 第 I 部 — プロジェクト宣言 (RULE_CODE 第 I 部)

### 採用アーキテクチャ / スタック

- **Node.js >= 22 / TypeScript (ESM)** — Web サービス既定スタック (RULE_TECH_STACK)。
- HTTP は Node 標準 `http` + 自前静的配信 + 小さな JSON API、リアルタイム経路は `ws`。
  フレームワークを入れない理由: ルートが少数で依存最小化 (§18) が勝つ。
- **入力注入と最前面監視は常駐 C# ヘルパプロセス** (`helper/ManusInputHelper.cs`)。
  .NET Framework 同梱の `csc.exe` で初回起動時にビルドし、stdin の UTF-8 JSON Lines で
  コマンドを受けて Win32 `SendInput` を発行、最前面変化は stdout へイベント行として push する。
  - 文字列は `KEYEVENTF_UNICODE` — スキャンコード・IME 状態・キーボード配列に依存せず
    日本語がそのまま入る。
  - 特殊キー / ショートカットは仮想キーコード (VK)。**物理配列は JIS 日本語 108 キー
    Qwerty を前提**とし、変換 / 無変換 / かな / 半角全角 / ¥ 等の JIS キーを持つ。
  - 最前面監視は `GetForegroundWindow` + `QueryFullProcessImageName` のポーリング
    (既定 500ms、変化時のみ通知)。ブラウザ (chrome / msedge / firefox / vivaldi / brave) は
    UI Automation でアドレスバーの URL を best-effort 取得 (取れない場合は url 無しで通知し
    debug ログに残す — 設定不備ではなく実行時 capability の劣化なので許容)。
  - Node ネイティブアドオン (robotjs 等) を使わない理由: node-gyp / ABI 追従が壊れやすく、
    csc.exe は Windows に常在するためビルド依存ゼロで決定的。
- **ショートカットプロファイル**: exe 名 / URL パターンで解決される JSON 定義。
  ボタン = キーコンボ / テキスト / ホイール / マウスボタン保持 / マクロ (列挙実行)。
  既定プロファイルはリポ内 `profiles/` (コミット)、ユーザ編集分は `data/profiles/`
  (未追跡) で、同 id は ユーザ側が優先。Web の編集 UI から HTTP API で保存する。
- **ゲームパッド (ViGEm 仮想 X360) は後回し** (neco 指示 2026-08-09)。プロトコル上の
  `pad.*` メッセージは予約のみ。実装時は `vigemclient` を optionalDependency で足し、
  ドライバ不在は明示エラー (§7.1 無言フォールバック禁止) とする。
- フロントエンドは TS を esbuild で bundle した静的ページ (keyboard / touchpad /
  shortcuts / editor)。SPA フレームワーク不使用の理由: 各面が独立 1 画面で依存最小化が勝つ。
- ログは `@ludiars/vestigium` (vendored `lib/vestigium`、file dep)。

### 目的と重視点

1. **iPhone を最大限活用** — インストール不要 (同一 LAN の URL を開くだけ)、
   ホーム画面追加で PWA 的に常用。最前面アプリに応じて **勝手に適切な操作面に変わる**。
2. **日本語入力の確実性** — iPhone 側 IME (フリック) で確定した文字列を Unicode 注入。
   Windows 側 IME の状態に一切依存しない。
3. **低遅延** — WebSocket 1 本 + 常駐ヘルパ。打鍵・タッチ移動ごとにプロセス spawn しない。
4. **LAN 公開の最小限の安全** — ペアリング PIN (salt 付きハッシュ保存) + HMAC トークン。

### 主要設計判断

| 判断 | 理由 |
|------|------|
| 注入は常駐ヘルパ 1 プロセス | spawn/打鍵は遅延・残骸の両面で不可。寿命は server が所有し全経路で kill (§10) |
| Unicode 注入と VK 注入を分離 | 文字は配列非依存で確実に、ショートカットは物理キー等価で。混ぜると IME で化ける |
| 最前面監視もヘルパに同居 | Win32 呼び出し面を 1 プロセスに集約。責務は「Win32 入出力境界」で単一 (SRP) |
| URL は UIA の best-effort | ブラウザ拡張の導入を要求しない。タイトルだけでは URL が取れないため UIA を第一候補に |
| プロファイルは JSON + 編集 UI | 「カスタム配置可能」要件。ファイル正本なので git/手編集とも共存できる |
| PIN は平文保存しない | HARNESS §1。検証のみなので salt+SHA-256。初期設定は `npm run set-pin -- <pin>` |
| bind は 0.0.0.0 (LAN) | iPhone から届くことが製品要件。loopback 縛りは適用外だが認可必須とセット |

## 第 II 部 — 構成

```
src/
  server.ts             エントリポイント (config → vestigium → helper → http+ws 配線)
  config.ts             manus.config.json ローダ (既定値ファイル・env override)
  http/static-server.ts  静的配信 + /health
  http/api.ts           /api/pair /api/profiles* の JSON API
  auth/pairing.ts       PIN 検証 (salt+SHA-256) + HMAC セッショントークン発行/検証
  ws/protocol.ts        クライアント⇄サーバ メッセージ schema (zod)
  ws/hub.ts             認可済み WS 接続の集合 + broadcast (foreground push)
  ws/session.ts         WS 接続の認可ハンドシェイク + メッセージディスパッチ
  inject/helper-manager.ts  C# ヘルパの build/spawn/監視/JSON-line RPC + イベント購読 (寿命所有者)
  inject/keyboard.ts    text → unicode コマンド / key+modifiers → VK シーケンス
  inject/jis-vk-map.ts  JIS 108 キー名 → Windows VK コード表
  inject/mouse.ts       相対移動 / ボタン / ホイール → ヘルパコマンド
  foreground/watcher.ts ヘルパの foreground イベント → 状態保持 + hub broadcast
  profiles/store.ts     profiles/ + data/profiles/ の読込・保存・検証
  profiles/resolver.ts  exe 名 / URL パターン → プロファイル解決
  actions/executor.ts   ボタン action (keys/text/wheel/mouseButton/macro) の実行
  cli/set-pin.ts        ペアリング PIN 設定 CLI
helper/ManusInputHelper.cs  SendInput + 最前面監視 (csc.exe でビルド)
profiles/               既定ショートカットプロファイル (JSON, コミット)
frontend/src/           keyboard / touchpad / shortcuts / editor / 共通 api の TS
public/                 静的 HTML/CSS (js は esbuild 出力、コミットしない)
```

### データ / 永続物 (spec/data/config.md 参照)

- `manus.config.json` (コミット可・非シークレット): port / bind / 監視間隔 / 動作既定。
- `profiles/*.json` (コミット): 既定ショートカットプロファイル。
- `data/profiles/*.json` (未追跡): ユーザ編集プロファイル (同 id で既定を上書き)。
- `data/pairing.json` (未追跡): PIN の salt+hash。
- `data/session-secret` (未追跡): トークン署名鍵 (初回起動時に乱数生成)。
- `data/helper/ManusInputHelper.exe` (未追跡): csc ビルド成果物。

### セキュリティ境界

- LAN 内前提。認可は PIN → 短命トークン (24h) → WS ハンドシェイク / API Bearer。
- PIN 未設定なら起動を拒否 (fail-fast、無言で認可無し公開にしない)。
- 注入コマンドはすべて schema 検証後にのみヘルパへ渡す。shell を経由しない。
- 最前面情報 (exe/タイトル/URL) は認可済みクライアントにのみ配信 (無認可 WS には出さない)。

## 残課題 (フォローアップ)

- Ally 実機での実経路確認 (キー/マウス/ショートカットが実アプリに入るか、URL 取得の実測)。
- ゲームパッド (ViGEm) 実装 — 後回し指示。着手時は spec/feature/gamepad.md を起こす。
- PROJECT-CODES.md への `Mn` 追記 (ユーザ承認待ち)。
- origin 公開 (GitHub App) は初回リリース判断後。
