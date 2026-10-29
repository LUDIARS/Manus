# Manus C++ 移行 (job cpp-node-20261030)

Node のサーバ (`src/`) と C# の常駐ヘルパ (`helper/ManusInputHelper.cs`) を、1 つの C++ の実行ファイル
`manus` (Windows 専用、`native/`) にまとめる。目的は **脆弱性の受け口を減らすこと** と **余分な依存を増やさないこと**。
速度は目的ではない。

- 画面 (`frontend/` のソースと `public/` の配信物) は移行しない。C++ のサーバが `public/` をそのまま静的に配信する。
- 経路・JSON の形・ポート (5390)・`/health`・loopback bind の既定値は変えない。
- クラウド (Linux GCC) では `SendInput` を偽の入力先に差し替え、コアとテストだけをビルド・実行する。
  実機 (Windows) の入力注入は本社で確かめる。

## 1. 依存の棚卸し

### 1.1 移行前

| 依存 | 種別 | 版 | 推移的依存 | 移行後 | 理由 |
|---|---|---|---|---|---|
| Node.js ランタイム (V8・libuv・llhttp・OpenSSL・zlib・Brotli・c-ares・nghttp2・undici・ICU ほか同梱) | 実行時 | >= 22 | 同梱部品 十数個 | **不要になった** (サービス実行時) | HTTP・WS・暗号・ファイル配信を C++ で持つ。Brotli/zlib/HTTP2/undici は Manus の機能で使っておらず、受け口だけが残っていた |
| `ws` | npm 実行時 | 8.21.3 | 0 | **置き換え** → Boost.Beast websocket | permessage-deflate は Node 版でも無効 (ws の既定)。C++ でも有効にしない |
| `zod` | npm 実行時 | 3.25.76 | 0 | **置き換え** → 手書きの検証 (`native/src/ws/protocol.cpp`・`native/src/config/config.cpp`) | スキーマは 2 つだけ。ライブラリを足さない |
| `lib/vestigium` (`@ludiars/vestigium`、`zod` に依存) | npm 実行時 (HEAD では既に未使用。`lib/vestigium/package.json` だけ残存) | 0.1.0 | 1 (`zod`) | **置き換え** → `native/src/log/` (JSONL の書き出しを C++ で実装、ファイル仕様は Vestigium §2 準拠) | |
| .NET Framework `csc.exe` + `System.Web.Extensions` (JavaScriptSerializer) | 実行時 (OS 同梱、初回起動時にビルド) | 4.x | — | **不要になった** | ヘルパプロセス・自動ビルド・JSON-line RPC をなくし、`SendInput` を直接呼ぶ |
| `@types/node` / `@types/ws` | npm 開発時 | 22.20.1 / 8.18.1 | 1 / 2 | 切り替え後に削除可 | サーバの TS が無くなる |
| `tsx` | npm 開発時 | 4.23.11 | 28 (うち 27 はプラットフォーム別の任意パッケージ) | 切り替え後に削除可 | `dev` / `set-pin` は `manus set-pin` に置き換え。golden 記録スクリプトの実行にだけ使う |
| `vitest` | npm 開発時 | 2.1.9 | 90 (うち 50 は任意) | 切り替え後に削除可 | テストは ctest に移した |
| `typescript` | npm 開発時 | 5.9.3 | 0 | **残す** (画面の型検査) | 画面は移行しない |
| `esbuild` | npm 開発時 (画面のビルド) | 0.24.2 | 25 (すべてプラットフォーム別の任意パッケージ) | **残す** (画面のビルド) | 画面は移行しない。人間の判断が要ること参照 |

- `package-lock.json` の数: 実行時 2 (`ws`, `zod`。推移的 0)、開発時 149、合計 151。
- 移行後の実行時 npm 依存: **0**。切り替え後に残る npm 依存は画面のビルド用の `esbuild` + `typescript` (推移的含め 27、うち 25 はプラットフォーム別の任意パッケージ)。

### 1.2 移行後 (C++)

| 依存 | 版・取得 | 用途 | 推移的依存 |
|---|---|---|---|
| C++ 標準ライブラリ (C++20) | — | | — |
| OS API | Win32 (`user32` の `SendInput`、`ws2_32`/`mswsock`) / POSIX (テスト用) | 入力注入・ソケット | — |
| Boost (Asio / Beast のヘッダのみ) | 1.87.0 `boost-1.87.0-b2-nodocs.zip` SHA256=`40efbb1f38b8cf186fd7bb12809f561ae1c233bae0f75146862e9bfce77d0d42` | HTTP/1.1・WebSocket・タイマ・シグナル | なし (Boost 内のヘッダのみ。ビルドしない) |
| nlohmann_json | 3.11.3 `json.tar.xz` SHA256=`d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d` | JSON の解析・出力 | なし (ヘッダのみ) |
| OpenSSL 3 (libcrypto のみ) | `find_package(OpenSSL 3 REQUIRED)` システム提供 | SHA-256・HMAC-SHA256・乱数・定数時間比較 | なし (libssl は使わない) |
| Lorica | `LUDIARS/Lorica` commit `cab4f12086e30484f249d1b4aa479bf7cdec13a9` (`Lorica::boundary` + `lorica_harden_target`) | 入力の課金・ビルドの防御 | なし |

移行前の実行時の外部依存: Node.js ランタイム (+同梱部品十数個) + npm 2 + .NET Framework の csc/JavaScriptSerializer。
移行後: Boost (ヘッダ) + nlohmann_json (ヘッダ) + OpenSSL libcrypto + Lorica の 4 つ (いずれも共通規約の許可表の範囲内)。
追加の依存はない。

## 2. 機能の対応表

| 元 (TS / C#) | C++ | 備考 |
|---|---|---|
| `src/server.ts` (起動・配線・シグナル・終了) | `native/src/app/main.cpp`, `native/src/app/manus_app.{hpp,cpp}`, `native/src/app/cli.{hpp,cpp}` | `manus serve` (既定) / `manus set-pin <pin>` |
| `src/config.ts` | `native/src/config/config.{hpp,cpp}` (解析・env 上書きの純関数), `native/src/config/config_loader.{hpp,cpp}` (ファイル・env) | `manus.config.json` の strict スキーマ・`MANUS_PORT`/`MANUS_BIND` |
| `src/auth/pairing.ts` (PIN 照合・ロック・トークン) | `native/src/auth/pairing.{hpp,cpp}` (業務判断), `native/src/crypto/crypto.{hpp,cpp}` (OpenSSL) | トークン形式 `<期限ms>.<HMAC-SHA256 hex>` は同一。Node で発行したトークンは C++ でも有効 |
| `src/auth/pairing.ts` (`storePin`・ファイル) | `native/src/auth/pairing_store.{hpp,cpp}` | `data/pairing.json` / `data/session-secret` の形式は同一 |
| `src/cli/set-pin.ts` | `native/src/app/cli.cpp` + `pairing_store` | `manus set-pin <pin>` |
| `src/http/api.ts` | `native/src/http/api.{hpp,cpp}` | `/health`, `POST /api/pair` |
| `src/http/static-server.ts` | `native/src/http/static_files.{hpp,cpp}` (パス規則・MIME・ETag の純関数), `native/src/http/static_server.{hpp,cpp}` (stat と応答), `native/src/http/url.{hpp,cpp}` | `public/` 配信・MIME・ETag・no-store |
| Node `http` + `ws` (受け口) | `native/src/transport/` (`server`, `http_connection`, `ws_connection`, `connection_tracker`, `limits`) | Boost.Beast |
| `src/ws/protocol.ts` (zod) | `native/src/ws/protocol.{hpp,cpp}` | 手書きの検証。zod と同じ受理条件 |
| `src/ws/session.ts` | `native/src/ws/session.{hpp,cpp}` | ack / pong / error の JSON は同一 |
| `src/inject/jis-vk-map.ts` | `native/src/inject/jis_vk_map.{hpp,cpp}` | |
| `src/inject/keyboard.ts`, `src/inject/mouse.ts` | `native/src/inject/keyboard.{hpp,cpp}`, `native/src/inject/mouse.{hpp,cpp}`, `native/src/inject/command.{hpp,cpp}` | |
| `src/inject/helper-manager.ts` (csc ビルド・spawn・RPC・再起動) | なし (不要になった) | ヘルパプロセスが無くなったため |
| `helper/ManusInputHelper.cs` (入力列の組み立て) | `native/src/inject/input_plan.{hpp,cpp}` (OS 非依存) | Unicode 注入・拡張キー・修飾キーの順序・ホイール |
| `helper/ManusInputHelper.cs` (`SendInput`) | `native/src/inject/win32_input_sink.{hpp,cpp}` (Windows のみ) | 非 Windows は `unavailable_input_sink` (helper=down) |
| (ログ: Node の console。旧 `lib/vestigium`) | `native/src/log/` | console + Vestigium 形式の JSONL (`logs/manus/YYYY-MM-DD.jsonl`) |
| (ファイル・環境変数の読み書き) | `native/src/platform/` (`file_read`: Lorica 課金付きの読み込み・一時ファイル + rename の書き込み、`environment`) | |
| `scripts/helper-smoke.mjs` | なし (不要になった) | ヘルパが無い |
| `scripts/build-frontend.mjs`, `frontend/`, `public/` | 移行しない | 画面 |

## 3. 安全の対応表

### 3.1 既存の防御 (同じ挙動で移植)

| 防御 (Node) | 場所 (Node) | C++ | 挙動 |
|---|---|---|---|
| loopback bind の既定 (`127.0.0.1`) | `config.ts` | `config.cpp` | 同じ。`MANUS_BIND` で上書きできるのも同じ |
| PIN 未設定なら起動拒否 (fail-fast) | `pairing.ts` | `pairing_store.cpp` → `main.cpp` | 同じ |
| PIN は salt+SHA-256 で保存、平文を持たない | `pairing.ts` | `pairing.cpp` (`hash_pin`) | 同じ形式 (`sha256("<salt>:<pin>")` hex)。保存は一時ファイル + rename (強化) |
| PIN 形式 `^\d{4,10}$` | `pairing.ts` | `pairing.cpp` (`is_valid_pin_format`) | 同じ |
| PIN 比較の時間を一定にする | `timingSafeEqual` | `CRYPTO_memcmp` | 同じ (長さ比較の後に定数時間比較) |
| 5 回連続失敗で 60 秒ロック、ロック中は `locked` (429) | `pairing.ts` | `pairing.cpp` | 同じ (プロセス全体で 1 つの状態) |
| トークン = 期限 + HMAC-SHA256、改竄・期限切れを拒否 | `pairing.ts` | `pairing.cpp` | 同じ形式・同じ鍵ファイル |
| WS はトークン無効なら close 4401 | `session.ts` | `ws_connection.cpp` + `manus_app.cpp` | 同じ (101 の後に close 4401 "unauthorized") |
| `/ws` 以外への upgrade はソケット破棄 | `server.ts` | `http_connection.cpp` | 同じ |
| 注入コマンドは schema 検証後にのみ実行 | `session.ts` + zod | `protocol.cpp` | 同じ受理条件 (text 4096・key 1..16 は UTF-16 単位で数える) |
| shell を経由しない | `helper-manager.ts` | — | 子プロセス自体が無くなった |
| 静的配信のパス検査 (public の外は 403) | `static-server.ts` | `static_files.cpp` | 強化: 区切り付きで判定 (Node は前方一致のため `public-xxx/` のような兄弟ディレクトリが通り得た)。`\`・`:`(NTFS ADS)・NUL・Windows の予約デバイス名を拒否 |
| `/api/pair` の本文は 4096 まで | `api.ts` | `http_connection.cpp` (Beast の body_limit) + `api.cpp` | 同じ上限 (Node は文字数、C++ はバイト数で数える = 同等以上に厳しい) |
| 静的ファイルは no-store | `static-server.ts` | `static_files.cpp` | 同じ |

### 3.2 新しく付けた上限・期限 (Node では無制限だったもの)

| 経路 | 上限・期限 | 超えたとき |
|---|---|---|
| 同時接続 | 64 | 受け付けた直後に切断 (拒否) |
| 同時 WS セッション | 16 | 503 で拒否 |
| HTTP ヘッダ | 16 KiB (Node の既定と同じ) | 431 で切断 |
| HTTP 本文 | 4096 B (全経路) | `/api/pair` は 400 `bad_request`、ほかは 413。切断 |
| 最初のリクエストの受信 | 60 秒 (Node の headersTimeout と同じ) | 切断 |
| keep-alive の次のリクエスト | 5 秒 (Node の keepAliveTimeout と同じ) | 切断 |
| 応答の送信 | 30 秒 | 切断 |
| WS ハンドシェイク | 30 秒 | 切断 |
| WS の無通信 | 300 秒 (途中で ping を送る。ブラウザは自動で pong を返す) | 切断 (画面は 1 秒後に再接続する) |
| WS メッセージ | 64 KiB (Node の ws は 100 MiB) | close 1009 |
| WS メッセージの JSON | 深さ 8・要素 1024 | `bad_message` |
| `/api/pair` 本文の JSON | 深さ 32・要素 512 | 400 `bad_request` |
| `manus.config.json` / `data/pairing.json` | 64 KiB・深さ 16・要素 1024 | 起動失敗 (fatal) |
| `data/session-secret` | 32〜4096 B | 起動失敗 (fatal。削除すると再生成) |
| 静的ファイル | 64 MiB | 500 |
| 1 メッセージから作る入力イベント | 16384 件 | `inject_failed` |
| `tokenTtlHours` | 1〜2,000,000,000 (JS の安全な整数の範囲) | 起動失敗 |

### 3.3 Lorica の課金を入れた入力経路

| 入力経路 | 課金 (`lorica::input_budget`) |
|---|---|
| WS メッセージ (`protocol.cpp`) | バイト・要素・確保・深さ (解析前にバイト、SAX で要素と深さ) |
| `/api/pair` の本文 (`api.cpp`) | 同上 |
| `manus.config.json` (`config_loader.cpp`) | ファイルの大きさ (読む前) + JSON |
| `data/pairing.json` / `data/session-secret` (`pairing_store.cpp`) | ファイルの大きさ (読む前) + JSON |
| 静的ファイル (`static_files.cpp`) | ファイルの大きさ (開く前) |
| 入力イベント列 (`input_plan.cpp`) | 件数 × 大きさ (`checked_multiply`) |
| URL のデコード (`url.cpp`) | デコード後の大きさ |

## 4. Node 版と意図的に変えた挙動

互換性テスト (`native/tests/golden_tests.cpp`) で差分として明記し、C++ 側の結果を検証している。

| 入力 | Node | C++ | 理由 |
|---|---|---|---|
| `GET /..%2fpublic-evil/x.js` (public の兄弟ディレクトリ) | 200 で配信 | 403 | Node の `startsWith(publicRoot)` は区切りを見ない前方一致だった |
| `key` = `constructor` / `__proto__` など | `Object.prototype` のメンバを VK として扱った | `inject_failed` (unknown key name) | 表に載った名前だけをキーとする |
| text に単独のサロゲート (`"\ud800"`) | 受理 | `bad_message` | 不正な UTF-16。nlohmann_json が解析時に拒否 |
| Int32 を超える移動量・ホイール量 | (C# ヘルパで失敗) | `inject_failed` (同じ文言) | ヘルパと同じ。golden の偽ヘルパは全部受けるため差分扱い |
| WS の返信順 | ack が後続の pong に追い越されることがあった | 受信順に処理 | 画面は seq で照合しているので影響なし |
| `bad_message` の message 本文 | zod / JSON.parse の文面 | C++ の文面 | 画面は表示しない。code と seq の有無は同じ |
| `Sec-WebSocket-Version: 8` | 受理 | 400 | Beast は 13 のみ |
| ETag の値 | mtime の小数部を含む 36 進 | 整数 ms の 36 進 | 形は同じ。no-store のため実害なし |
| `MANUS_PORT` が数値でない・0 | listen が例外 (0 は空きポート) | 起動失敗 (1..65535 の整数のみ) | |
| `data/session-secret` が 32 B 未満 | そのまま使用 | 起動失敗 | 短い鍵を使わない |

## 5. テスト

### 5.1 元の TS テストとの対応

| TS (vitest) | C++ (ctest) |
|---|---|
| `src/auth/pairing.test.ts` 「PIN 未設定なら fail-fast する」 | `manus.pairing` `fails_fast_without_pin` |
| 〃 「正しい PIN でトークンを発行し検証が通る」 | `manus.pairing` `correct_pin_issues_verifiable_token` |
| 〃 「誤った PIN は拒否し、5 回でロックする」 | `manus.pairing` `wrong_pin_rejected_and_locks_after_five` |
| 〃 「改竄トークンを拒否する」 | `manus.pairing` `tampered_token_rejected` |
| 〃 「期限切れトークンを拒否する」 | `manus.pairing` `expired_token_rejected` |
| `src/inject/jis-vk-map.test.ts` 「英字は ASCII 大文字の VK」 | `manus.jis_vk_map` `letters_are_ascii_uppercase_vk` |
| 〃 「矢印・ファンクション・JIS 固有キーを持つ」 | `manus.jis_vk_map` `arrows_function_and_jis_keys` |
| 〃 「VK が重複しない」 | `manus.jis_vk_map` `no_duplicate_vks_except_capslock_eisu` |
| 〃 「未知キーは投げる」 | `manus.jis_vk_map` `unknown_key_has_no_vk` |
| `src/inject/keyboard.test.ts` textCommand 「文字列をそのまま text op にする」 | `manus.keyboard` `text_command_keeps_the_string` |
| 〃 keyCommand 「修飾キーを先頭に、対象キーを末尾に並べる」 | `manus.keyboard` `modifiers_first_key_last` |
| 〃 「複数修飾は指定順で先頭に積む」 | `manus.keyboard` `multiple_modifiers_keep_their_order` |
| 〃 「action を透過する」 | `manus.keyboard` `action_passes_through` |
| 〃 「未知キー名は投げる」 | `manus.keyboard` `unknown_key_name_is_an_error` |
| `spec/test/test-plan.md` ws/protocol (未実装だった) | `manus.protocol` |
| `spec/test/test-plan.md` inject/keyboard の text→op (未実装だった) | `manus.keyboard` `text_plan_newline_tab_and_surrogates` |
| (なし: C# ヘルパの入力列) | `manus.input_plan`、Windows のみ `manus.win32_input_sink` |

追加: `manus.util` (Lorica 課金付き JSON・JS の数値表記・UTF)、`manus.url`、`manus.config`、`manus.log`、`manus.api`、
`manus.session`、`manus.static_files`、`manus.server` (受け口の上限・期限・同時数)、`manus.app` (同一プロセスの結合)、
`manus.golden` (Node 実装との互換性)。

### 5.2 互換性テスト (golden)

`native/tests/tools/record-golden.ts` が Node 実装 (src/) を直接動かして `native/tests/fixtures/golden/*.json` を作る
(C# ヘルパは記録用の偽物に置き換え、HTTP は 127.0.0.1 の空きポート、ペアリングのファイルは一時ディレクトリ)。

```sh
npm ci --ignore-scripts
npx tsx native/tests/tools/record-golden.ts
ctest --test-dir native/build -R manus.golden --output-on-failure
```

### 5.3 本社 (Windows 実機)

`pwsh -File native/tests/tools/honsha-check.ps1 [-Smoke]` — MSVC で警告をエラーにしてビルド、golden の再記録、
ctest (SendInput を偽の関数に差し替えた `manus.win32_input_sink` を含む)、`-Smoke` で一時ディレクトリに
set-pin して起動し `/health`・`/api/pair`・静的配信を確かめる (入力は注入しない)。
実機での入力注入は `spec/test/test-plan.md` の「実経路」の手順 (iPhone からペアリング → メモ帳へ日本語入力・
タッチパッド・遅延 HUD) を C++ 版で行う。ヘルパの kill → 再起動の項目はヘルパが無くなったため対象外。

## 6. ビルド

```sh
cmake -S native -B native/build -DCMAKE_BUILD_TYPE=Release
cmake --build native/build -j2
ctest --test-dir native/build --output-on-failure
```

依存の取得は FetchContent (URL + `URL_HASH SHA256`)。ローカルの取得済みソースを使うときは
`-DFETCHCONTENT_SOURCE_DIR_BOOST=...` `-DFETCHCONTENT_SOURCE_DIR_JSON=...` `-DLORICA_SOURCE_DIR=...` を渡す。
`-DMANUS_WARNINGS_AS_ERRORS=ON` で警告をエラーにする。

実行: `native/build/bin/manus [serve] [--root <dir>]` (既定の root は作業ディレクトリ = リポのルート)。
`set-pin` は `native/build/bin/manus set-pin <4〜10桁の数字>`。Windows 以外では入力を注入できない
(`/health` は helper=down、注入は `helper_down`)。
