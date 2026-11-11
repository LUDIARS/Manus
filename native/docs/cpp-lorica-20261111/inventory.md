# Manus 棚卸し (job cpp-lorica-20261111, W5)

ブランチ `cpp/manus-node-port`、着手時の先頭 `75633e7` (main より 9 commit 先)。

## 1. ビルド単位 (着手時)

`native/CMakeLists.txt` の関数 `manus_target()` が `manus_warnings` と `lorica_harden_target(<t>)` (baseline) を付ける。
`native/src/CMakeLists.txt` / `native/tests/CMakeLists.txt` は全ビルド単位でこの関数を呼ぶ。
指示書は「`native/src/CMakeLists.txt` にも Lorica が入っている」と書くが、実物では Lorica の取り込みと固定は
`native/CMakeLists.txt` だけで、`native/src/CMakeLists.txt` は `manus_target()` 経由で `lorica_harden_target` を呼んでいる
(と `manus_core` が `Lorica::boundary` を PUBLIC で link)。まとめ役の検索結果と一致。

| ビルド単位 | 種別 | Linux でビルド | Lorica::boundary | harden | deprecate_unsafe_apis |
|---|---|---|---|---|---|
| manus_core | static lib | 可 | 直接 (PUBLIC) | baseline | なし |
| manus_io | static lib | 可 (Linux は unavailable_input_sink、Windows は win32_input_sink) | manus_core 経由のみ | baseline | なし |
| manus_transport | static lib | 可 | 経由のみ | baseline | なし |
| manus_app | static lib | 可 | 経由のみ | baseline | なし |
| manus (常駐サーバ) | 実行ファイル | 可 (注入不可。helper=down) | 経由のみ | baseline | なし |
| manus_test_main | static lib | 可 | **なし** | baseline | なし |
| manus_{util,url,config,log,server,pairing,api,jis_vk_map,keyboard,input_plan,protocol,session,app,static_files,golden}_tests (15) | テスト | 可 | 経由のみ | baseline | なし |
| manus_win32_input_sink_tests | テスト | **不可 (WIN32 のみ)** | 経由のみ | baseline | なし |

Lorica の固定: `cab4f12086e30484f249d1b4aa479bf7cdec13a9` (→ 共通の `5e8281b0…` へそろえる)。`LORICA_BUILD_PROTECTION OFF`。

## 2. 外から入る入力の経路

| 経路 | 場所 | 着手時の上限 |
|---|---|---|
| HTTP ヘッダ・request-target | `transport/http_connection.cpp` | Beast header_limit 16 KiB |
| HTTP 本文 (全経路、主に `POST /api/pair`) | `transport/http_connection.cpp` → `http/api.cpp` | Beast body_limit 4096 → `parse_bounded_json` (内部で budget を 2 つ作る) |
| URL のデコード (pathname・query の token) | `http/url.cpp` | input_budget (デコード後の大きさ) |
| WebSocket のメッセージ (フレーム) | `transport/ws_connection.cpp` → `ws/session.cpp` → `ws/protocol.cpp` | Beast read_message_max 64 KiB (flat_buffer 自体は上限なし) → `parse_bounded_json` |
| ペアリングの PIN (本文の `pin`) | `http/api.cpp` → `auth/pairing.cpp` | 本文の上限のみ (PIN の長さは見ずに SHA-256) |
| セッショントークン (`/ws?token=`) | `app/manus_app.cpp` → `auth/pairing.cpp verify_token` | URL の上限のみ (長さを見ずに HMAC) |
| 設定ファイル `manus.config.json` | `config/config_loader.cpp` | file_read の budget + JSON |
| `data/pairing.json` / `data/session-secret` | `auth/pairing_store.cpp` | 同上 |
| 静的ファイル (`public/`) | `http/static_server.cpp` | 64 MiB の budget |
| 環境変数 `MANUS_PORT` / `MANUS_BIND` | `config/config.cpp` | 形式検査 |
| 入力イベント列 (派生データ) | `inject/input_plan.cpp` | checked_multiply + budget 16384 件 |

## 3. 依存 (前回のブランチが使っているもの)

C++: Boost 1.87.0 (Asio/Beast ヘッダ、URL_HASH 済)、nlohmann_json 3.11.3 (URL_HASH 済)、OpenSSL 3 libcrypto (システム)、
Lorica、Threads、Win32 `user32`/`ws2_32`/`mswsock`。**すべて許可表の範囲内。許可表に当たらない依存は無い。**
npm (Node 版、今回は消さない): 実行時 `ws` `zod`、開発時 `@types/node` `@types/ws` `esbuild` `tsx` `typescript` `vitest`。

## 4. 前回の残り

- 機能の移植は `spec/plan/cpp-port.md` の表で全部対応済み (画面は移さない)。
- 残り: Lorica 固定の更新、テストのビルド単位への Lorica::boundary の直接 link、`lorica_deprecate_unsafe_apis`、EXTENDED、
  sanitizer の CMake option (前回は手で ASan/UBSan を回しただけ)、fuzz の入口、`SendInput` の呼び出し規則が Windows でしか
  テストできない (adapter の分割)、PIN とトークンの長さの上限、WS の受信バッファの上限、1 依頼 1 budget への整理。
