# Manus の状況 (job cpp-lorica-20261111、2 代目の担当の仕上げ時点)

ブランチ `cpp/manus-node-port` (未 push)。着手時の先頭 `75633e7`。引き継ぎ時の先頭 `3f77b3f`。

| SHA | 内容 | 確認 |
|---|---|---|
| `357df97` | Lorica の固定を `5e8281b0e60fa16f814e16eaa620715937e8618d` へそろえる | 前任: GCC ビルド + ctest 15/15 |
| `78ebbbb` | Lorica を全ビルド単位へ適用 (boundary・baseline・unsafe API の拒否・常駐サーバは EXTENDED) | 前任: GCC (-Werror) + ctest 15/15 + verify_binary |
| `856e44e` | 入力の経路ごとの上限チェックと境界テスト (WS・ペアリングと PIN・HTTP の本文) | 前任: GCC (-Werror, EXTENDED) + ctest 15/15 |
| `76cb67e` | WIP: fuzz の入口と SendInput の呼び出し規則の分離 | 下の `fbb15a6`・`2849e58` で接続して確かめた |
| `3f77b3f` | 引き継ぎ文書 | — |
| `fbb15a6` | WIP の仕上げ: batch_dispatch を manus_core とテストに登録 | GCC -Werror ctest 21/21 (次と合わせて) |
| `2849e58` | `MANUS_SANITIZE`・`MANUS_BUILD_FUZZERS`、fuzz の回帰を ctest へ | GCC 21/21、GCC ASan+UBSan 21/21、Clang TSan 21/21、Clang ASan+UBSan+fuzzer 21/21、fuzz 5 入口 x 30 秒 |
| `032c6d7` | `.anatomia/layers.json` に native/ の層を宣言 (要確認) | `anatomia domains program` で native/ の未分類 0 |
| (この文書のコミット) | 文書 (status.md・spec/plan/cpp-port.md §3.3 と §6) | — |

## (a) 済んで確かめたこと (2 代目の環境: Linux x86_64、GCC 13.3、Clang 18.1.3、CMake 3.28、Ninja、OpenSSL 3.0.13)

- 引き継ぎ時の先頭 `3f77b3f` で configure → build (GCC, RelWithDebInfo, -Werror, EXTENDED) → ctest 15/15。
- 最終状態 (GCC, RelWithDebInfo, -Werror, EXTENDED): ctest **21/21** (15 + manus.batch_dispatch + manus.fuzz.* 5 件)。
- GCC 13 `-DMANUS_SANITIZE=address,undefined`: ctest 21/21 (sanitizer の報告なし)。
- Clang 18 `-DMANUS_SANITIZE=thread`: ctest 21/21 (TSan の報告なし)。manus.server / app / golden は 5 回繰り返しも実行 (結果は報告)。
- Clang 18 `-DMANUS_SANITIZE=address,undefined -DMANUS_BUILD_FUZZERS=ON`: ctest 21/21。libFuzzer 5 入口を各 30 秒:
  ws_message 56 万回・pair_body 74 万回・url 104 万回・config 136 万回・http_request 165 万回、クラッシュ・リーク・タイムアウトなし。
- `compile_commands.json` (GCC 最終): Manus の TU 58 個すべてに `-D_FORTIFY_SOURCE=3`・`-D_GLIBCXX_ASSERTIONS`・`-fstack-protector-strong`・
  `-fstack-clash-protection`・`-fcf-protection=full`・`-include lorica/unsafe_api.hpp`・`-Werror=deprecated-declarations`。
  常駐サーバの TU 31 個に `-ftrivial-auto-var-init=zero` (EXTENDED)。打ち消す設定は無い。
- `verify_binary.py --readelf /usr/bin/readelf native/build/bin/manus`: `ELF PIE/NX-stack/full-RELRO metadata present
  (instruction coverage not verified)`、終了コード 0 (最終状態のバイナリ)。
- `win32_input_sink.cpp` と `win32_input_sink_tests.cpp`: windows.h の簡易な代用ヘッダ (LLP64 の幅に合わせた型・定数・関数宣言だけ) で
  `g++` と `clang++` の `-fsyntax-only -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror` が通る。
  これは構文と型の幅の確認だけで、Windows SDK・MSVC での確認ではない。

## (b) 確かめていないこと (本社で行う)

- Windows (MSVC) のビルド・警告なし・EXTENDED (MSVC 19.44 以上)・`manus.win32_input_sink`・`manus.batch_dispatch`・実機での入力注入。
  手順: `pwsh -File native/tests/tools/honsha-check.ps1 [-Smoke]`。
- MSVC の `-DMANUS_SANITIZE=address,undefined` (/fsanitize=address)。
- GCC の TSan (`-Wno-error=tsan` にしたビルドでの ctest)。TSan の記録は Clang で取った。
- golden の再記録 (Node 側は変えていない)。

## (c) 途中のこと

なし (WIP `76cb67e` は `fbb15a6`・`2849e58` で接続した)。

## (d) 未着手

なし (クラウドで行える範囲)。

## (e) 分かっている失敗・振る舞いの変化

- 既定の Linux ビルドと ctest に落ちるものは無い。
- GCC の `-fsanitize=thread` は `-Werror` のままだと Boost.Asio の `atomic_thread_fence` で `-Wtsan` が止める。
  `MANUS_SANITIZE=thread` かつ GCC のときだけ `-Wno-error=tsan` (警告は残る)。
- このコンテナでは `/home/user/Lorica` が `/home/user/lorica` への symlink のため、Clang が `-include` のパスの大文字小文字の違いで
  `-Wnonportable-include-path` を出す。Clang のビルドは実際のパス (`/home/user/manus`・`/home/user/lorica`) で行った。リポの問題ではない。
- 途中で試して戻したもの (前任): WS の `flat_buffer::max_size(64 KiB)` は close 1009 にならないのでやめ、`read_message_max` だけで上限を持つ。

## (f) 人間の判断が要ること

1. `MANUS_HARDEN_EXTENDED` を既定 ON にした。MSVC 19.44 未満・x64 以外では configure が失敗する。更新するか `-DMANUS_HARDEN_EXTENDED=OFF` にするか。
2. Lorica の `sealed_value` (`Lorica::protection`) は使っていない (`LORICA_BUILD_PROTECTION OFF`)。上限値は `constexpr` で十分と判断。
3. コミットの末尾の署名行: COMMON-2.md の文言 (`Claude Fable 5.2`) ではなく、このセッションの帰属設定の 2 行を使った。前任の分も帰属設定に合わせている。
4. 指示書は「`native/src/CMakeLists.txt` にも Lorica が入っている」と書くが、取り込みと固定は `native/CMakeLists.txt` だけ。
5. 許可表に当たらない依存は無い。追加した依存も無い。
6. Revisor の「413 件の変更アンカーが未分類」は spec/domains ではなく `.anatomia/layers.json` (無かった) が原因。
   `032c6d7` で native/ の層を宣言し `.gitignore` で追跡するようにしたが、層の割り当て (特に transport を presentation、
   auth/http/inject を domain としたこと) と、`.anatomia/` を追跡し始めること自体の確認が要る。不要ならこのコミットだけ外せる。
   Node 版 (src/ frontend/ helper/) は宣言していない (このブランチで変えていない)。

## (g) いま時点の一覧

### Lorica の適用表 (固定 `5e8281b0e60fa16f814e16eaa620715937e8618d`)

すべて `native/CMakeLists.txt` の `manus_target()` 経由。

| ビルド単位 | Lorica::boundary | harden | deprecate_unsafe_apis | Linux でビルド |
|---|---|---|---|---|
| manus_core / manus_io / manus_transport / manus_app | PRIVATE (core は PUBLIC も) | EXTENDED | あり | 済 |
| manus (常駐サーバ) | PRIVATE | EXTENDED | あり | 済。verify_binary: PIE/NX/full-RELRO |
| manus_test_main | PRIVATE | baseline | あり | 済 |
| テスト 16 個 (manus_*_tests、batch_dispatch を含む) | PRIVATE | baseline | あり | 済 |
| manus_win32_input_sink_tests | PRIVATE | baseline | あり | 不可 (WIN32 のみ・未確認) |
| fuzz の回帰 5 個 (manus_fuzz_*_regression) | PRIVATE | baseline | あり | 済 (GCC・Clang) |
| libFuzzer の実行ファイル 5 個 (manus_fuzz_*、MANUS_BUILD_FUZZERS=ON) | PRIVATE | baseline | あり | 済 (Clang 18) |

対象外: OpenSSL libcrypto (システムのビルド済み)、Boost・nlohmann_json (ヘッダのみ。Manus の TU の中で Manus の設定でコンパイルされる)。
`lorica_deprecate_unsafe_apis` を入れられなかった箇所: なし。

### 上限チェックを入れた場所 (`856e44e` の行番号。最新の表は `spec/plan/cpp-port.md` §3.3)

| 経路 | 場所 | 部品 | 上限 | 根拠 |
|---|---|---|---|---|
| WS メッセージ (transport) | `native/src/transport/ws_connection.cpp:35` / `:72` | Beast read_message_max + `byte_view` | 64 KiB、超えたら close 1009 | 前回の値 (`transport/limits.hpp`) |
| WS メッセージ (session) | `native/src/ws/session.cpp:68-69` | `input_budget` (1 メッセージ 1 つ) + `byte_view` | 64 KiB・要素 1024・深さ 8 | `ws/protocol.hpp:60` |
| WS の JSON | `native/src/ws/protocol.cpp:158-165` → `util/bounded_json.cpp:106` | 同じ budget | 同上 | 同上 |
| JSON の budget の大きさ | `native/src/util/bounded_json.cpp:99-101` | `checked_multiply` / `checked_add` | 生 + 展開後 = 2 x max_bytes | 展開後の文字列は生の大きさを超えない |
| HTTP の本文 (transport) | `native/src/transport/http_connection.cpp:88` / `:162` | Beast body_limit + `byte_view` | 4096 B (/api/pair は 400、他は 413) | api.ts の readBody |
| HTTP の読み取りバッファ | `native/src/transport/http_connection.cpp:56-57` | `checked_add` | ヘッダ 16 KiB + 本文 + 64 KiB | 前回の値 |
| POST /api/pair | `native/src/http/api.cpp:25-26` | `input_budget` (1 依頼 1 つ) + `byte_view` | 4096 B・要素 512・深さ 32 | `api.cpp:15` (前回の値) |
| PIN の試行 | `native/src/auth/pairing.cpp:53` (`:14` の fits) | `byte_view` | 64 B (超えたらハッシュせず bad_pin、ロックの数には入る) | 保存される PIN は 4〜10 桁。余裕を持たせて決めた |
| セッショントークン | `native/src/auth/pairing.cpp:73` | `byte_view` | 128 B (超えたら MAC を計算せず無効) | 発行するトークンは最大 83 B |
| URL のデコード | `native/src/http/url.cpp:172` | `input_budget` | 入力の大きさ | 前回から |
| 設定・ペアリングのファイル | `native/src/platform/file_read.cpp:18`、`config/config.hpp:27`、`auth/pairing_store.cpp:12,14` | `input_budget` | 64 KiB・要素 1024・深さ 16、secret 4096 B | 前回から |
| 静的ファイル | `native/src/http/static_server.cpp:45` | `input_budget` | 64 MiB | 前回から |
| 入力イベント列 | `native/src/inject/input_plan.cpp:51-84` | `checked_multiply` + `input_budget` | 16384 件 | 前回から |
| SendInput の 1 回の呼び出し | `native/src/inject/batch_dispatch.cpp:15-18` | `checked_multiply` | 件数 x sizeof(INPUT) <= INT_MAX | SendInput の引数が int |

## (h) 次にやる順番

1. まとめ役の受け入れテスト (下の (i))。
2. 本社 (Windows): `pwsh -File native/tests/tools/honsha-check.ps1 -Smoke`。続けて実機での入力注入 (`spec/test/test-plan.md` の「実経路」)。
3. (f) の判断。

## (i) ローカルのビルドとテストの手順

前提: GCC 12 以上 (または Clang 18)、CMake 3.24 以上、Ninja、OpenSSL 3 の開発パッケージ、Python 3 (verify_binary)。

```sh
cd /home/user/Manus
export CCACHE_DIR=/home/user/.cpp-deps/ccache   # 任意
cmake -S native -B native/build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DMANUS_WARNINGS_AS_ERRORS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DLORICA_SOURCE_DIR=/home/user/Lorica \
  -DFETCHCONTENT_SOURCE_DIR_BOOST=/home/user/.cpp-deps/src/boost-1.87.0 \
  -DFETCHCONTENT_SOURCE_DIR_JSON=/home/user/.cpp-deps/src/nlohmann_json/json
/home/user/.cpp-deps/bin/buildslot cmake --build native/build -j2
/home/user/.cpp-deps/bin/buildslot ctest --test-dir native/build --output-on-failure   # 期待: 21/21
python3 -I /home/user/Lorica/tools/verify_binary.py --readelf /usr/bin/readelf native/build/bin/manus
```

sanitizer と fuzz (Clang は symlink を通さない実際のパスで。上の `-D` に足す):

```sh
# ASan+UBSan (GCC): -B native/build-asan -DMANUS_SANITIZE=address,undefined → ctest 21/21
# TSan (Clang):     -S /home/user/manus/native -B /home/user/manus/native/build-tsan -DCMAKE_CXX_COMPILER=clang++ \
#                   -DLORICA_SOURCE_DIR=/home/user/lorica -DMANUS_SANITIZE=thread → ctest 21/21
# fuzz (Clang):     同じく -B .../build-fuzz -DMANUS_SANITIZE=address,undefined -DMANUS_BUILD_FUZZERS=ON → ctest 21/21、
#                   .../build-fuzz/fuzz/manus_fuzz_<入口> -max_total_time=30 <作業用 corpus> native/fuzz/corpus/<入口>
```

option: `MANUS_HARDEN_EXTENDED` (既定 ON)、`MANUS_WARNINGS_AS_ERRORS` (既定 OFF)、`MANUS_SANITIZE` (既定は空)、`MANUS_BUILD_FUZZERS` (既定 OFF)。
