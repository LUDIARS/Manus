# Manus の状況 (job cpp-lorica-20261111、引き継ぎ時点)

ブランチ `cpp/manus-node-port` (未 push)。着手時の先頭 `75633e7`。この job で積んだコミット:

| SHA | 内容 | 確認 |
|---|---|---|
| `357df97` | Lorica の固定を `5e8281b0e60fa16f814e16eaa620715937e8618d` へそろえる | GCC ビルド + ctest 15/15 |
| `78ebbbb` | Lorica を全ビルド単位へ適用 (boundary・baseline・unsafe API の拒否・常駐サーバは EXTENDED) | GCC ビルド (-Werror) + ctest 15/15 + verify_binary |
| `856e44e` | 入力の経路ごとの上限チェックと境界テスト (WS・ペアリングと PIN・HTTP の本文) | GCC ビルド (-Werror, EXTENDED) + ctest 15/15 |
| `76cb67e` | **WIP(未完・未検証)**: fuzz の入口と SendInput の呼び出し規則の分離 (CMake に未接続) | なし |
| (この文書のコミット) | 引き継ぎ文書 | — |

## (a) 済んで確かめたこと

環境: Linux x86_64、GCC 13.3、CMake 3.28、Ninja、OpenSSL 3.0.13。手順は (i)。

- `cmake --build native/build -j2` (RelWithDebInfo, `-DMANUS_WARNINGS_AS_ERRORS=ON`, EXTENDED 既定 ON): 警告なしで成功 (`856e44e` の時点)。
- `ctest --test-dir native/build --output-on-failure`: **15/15 通過** (manus.util 14 件・session 10 件・pairing 16 件・api 8 件・server 22 件を含む。この job で足した境界テスト 7 件はすべて `[ ok ]` を目で確かめた)。
- `compile_commands.json`: `manus` の TU に `-D_FORTIFY_SOURCE=3`・`-D_GLIBCXX_ASSERTIONS`・`-D_LIBCPP_HARDENING_MODE=..._EXTENSIVE`・
  `-ftrivial-auto-var-init=zero`・`-fstack-protector-strong`・`-fstack-clash-protection`・`-fcf-protection=full`・
  `-include lorica/unsafe_api.hpp -Werror=deprecated-declarations` が付く。テストは baseline (FAST)。
  守りを打ち消す設定は無い (`-U_FORTIFY_SOURCE` は Lorica 自身が直後に `-D_FORTIFY_SOURCE=3` を付けるためのもの)。
- `python3 -I /home/user/Lorica/tools/verify_binary.py --readelf readelf native/build/bin/manus`:
  `ELF PIE/NX-stack/full-RELRO metadata present (instruction coverage not verified)`、終了コード 0 (`78ebbbb` の時点のバイナリ)。
- configure だけ: Debug・ビルド種別なし・Clang 18 (RelWithDebInfo) のいずれも EXTENDED 込みで通る (ビルドはしていない)。

## (b) 済んだが確かめていないこと

- `856e44e` の後のバイナリでの `verify_binary.py` の再実行 (CMake の設定は同じなので結果は変わらない見込みだが未実行)。
- `win32_input_sink.cpp` (WIP) と `win32_input_sink_tests.cpp`: windows.h の簡易な代用ヘッダで `clang++ -fsyntax-only` を通しただけ。MSVC・Windows SDK では未確認。
- Windows (MSVC) でのビルド・警告なし・EXTENDED (MSVC 19.44 以上が要る)・`manus.win32_input_sink`・実機での入力注入: すべて未実行 (本社で行う)。

## (c) 途中のこと (WIP コミット `76cb67e`)

- `native/fuzz/`: 入口 5 つ (`fuzz_ws_message` `fuzz_pair_body` `fuzz_url` `fuzz_config` `fuzz_http_request`)、`regression_main.cpp`
  (種を 1 回ずつ流す。GCC でも動く形)、`corpus/`、`fuzz/CMakeLists.txt` (関数 `manus_add_fuzz`)。
  **未接続**: `native/CMakeLists.txt` に `add_subdirectory(fuzz)` が無い。`option(MANUS_BUILD_FUZZERS ...)` と、
  ON のとき全 manus ターゲットへ `-fsanitize=fuzzer-no-link` を足す処理が未定義。一度もコンパイルしていない。
- `native/src/inject/batch_dispatch.{hpp,cpp}`: SendInput の呼び出しの規則 (空なら呼ばない・`checked_multiply(件数, sizeof(INPUT)) <= INT_MAX`・
  短い件数のエラー文 `SendInput injected n/total (err=e)`) を OS 非依存にしたもの。**`src/CMakeLists.txt` の manus_core に未登録。**
- `native/src/inject/win32_input_sink.cpp`: batch_dispatch を使う形に書き換え済み。
  **batch_dispatch.cpp を manus_core に登録するまで、Windows のビルドはリンクで失敗する。** Linux の既定ビルドには影響しない。
- `native/tests/batch_dispatch_tests.cpp`: 境界テスト 4 件。**`tests/CMakeLists.txt` に未登録** (`manus_add_test(batch_dispatch manus_core)` を足す)。
- sanitizer の CMake option は未コミット (下の (h) に入れる内容を書いた)。

## (d) 未着手

- `MANUS_SANITIZE` の option と ASan/UBSan・TSan での ctest (前回の job で手で ASan/UBSan を回した記録はある)。
- fuzz の短時間 (数十秒) の実行。
- golden の再記録 (`native/tests/tools/record-golden.ts`。Node 側は変えていないので不要の見込み)。
- `spec/plan/cpp-port.md` §3.3 (Lorica の課金を入れた経路の表) を (g) の内容に更新すること。
- 報告 `/home/user/.cpp-deps/reports/Manus-report.md` (指示書 11 章の形)。

## (e) 分かっている失敗

- 既定の Linux ビルドと ctest に落ちるものは無い (`856e44e`・`76cb67e` とも 15/15)。
- WIP の `win32_input_sink.cpp` は Windows でリンクエラーになる (上記 (c))。
- 途中で試して戻したもの: WS の `flat_buffer::max_size(64 KiB)` を設定すると、Beast が 1009 で閉じずに buffer_overflow で切るため
  `manus.server` の `websocket_message_limit_exact_and_plus_one` と `fragmented_message_over_limit_is_refused` が落ちた。
  設定はやめ、`read_message_max` だけで上限を持つ (ws_connection.cpp に理由を書いた)。

## (f) 人間の判断が要ること

1. `MANUS_HARDEN_EXTENDED` を既定 ON にした。MSVC 19.44 未満・x64 以外の環境では configure が失敗する (守りを外して通さない)。
   本社の MSVC が古い場合、更新するか `-DMANUS_HARDEN_EXTENDED=OFF` で baseline にするかを決める。
2. Lorica の `sealed_value` (`Lorica::protection`) は使っていない (`LORICA_BUILD_PROTECTION OFF` のまま)。上限値は `constexpr` で十分と判断。
3. コミットの末尾の署名行は、COMMON.md 6 の文言ではなく、このセッションの帰属設定と前回の作業ブランチのコミットに合わせた。違いの確認をまとめ役に頼む。
4. 指示書は「`native/src/CMakeLists.txt` にも Lorica が入っている」と書くが、取り込みと固定は `native/CMakeLists.txt` だけ。`native/src/CMakeLists.txt` は `manus_target()` を呼ぶだけ。
5. 許可表に当たらない依存は無い。追加した依存も無い。

## (g) いま時点の一覧

### Lorica の適用表 (固定 `5e8281b0e60fa16f814e16eaa620715937e8618d`)

すべて `native/CMakeLists.txt` の `manus_target()` 経由。

| ビルド単位 | Lorica::boundary | harden | deprecate_unsafe_apis | Linux でビルド |
|---|---|---|---|---|
| manus_core / manus_io / manus_transport / manus_app | PRIVATE (core は PUBLIC も) | EXTENDED | あり | 済 |
| manus (常駐サーバ) | PRIVATE | EXTENDED | あり | 済。verify_binary: PIE/NX/full-RELRO |
| manus_test_main | PRIVATE | baseline | あり | 済 |
| テスト 15 個 (manus_*_tests) | PRIVATE | baseline | あり | 済 |
| manus_win32_input_sink_tests | PRIVATE | baseline | あり | 不可 (WIN32 のみ・未確認) |
| fuzz の入口と regression (WIP) | `manus_target()` を呼ぶ形で書いたが未接続 | — | — | 未 |

対象外: OpenSSL libcrypto (システムのビルド済み)、Boost・nlohmann_json (ヘッダのみ。Manus の TU の中で Manus の設定でコンパイルされる)。
`lorica_deprecate_unsafe_apis` を入れられなかった箇所: なし。

### 上限チェックを入れた場所 (`856e44e` の行番号)

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

## (h) 次にやる順番

1. WIP を仕上げる: `src/CMakeLists.txt` の manus_core に `inject/batch_dispatch.cpp` を足す (Windows のリンクエラーの解消)。
   `tests/CMakeLists.txt` に `manus_add_test(batch_dispatch manus_core)`。ビルドと ctest。
2. sanitizer の option を `native/CMakeLists.txt` の `manus_target()` の前に足し、`manus_target()` の link に `manus_sanitize` を加える:
   ```cmake
   set(MANUS_SANITIZE "" CACHE STRING "Sanitizers for every Manus target: empty, address,undefined or thread")
   add_library(manus_sanitize INTERFACE)
   if(MANUS_SANITIZE)
     if(MSVC)
       target_compile_options(manus_sanitize INTERFACE /fsanitize=address)  # address のみ
     elseif(MANUS_SANITIZE STREQUAL "address,undefined" OR MANUS_SANITIZE STREQUAL "thread")
       target_compile_options(manus_sanitize INTERFACE -fsanitize=${MANUS_SANITIZE} -fno-omit-frame-pointer -fno-sanitize-recover=all)
       target_link_options(manus_sanitize INTERFACE -fsanitize=${MANUS_SANITIZE})
     else()
       message(FATAL_ERROR "MANUS_SANITIZE must be empty, address,undefined or thread")
     endif()
   endif()
   ```
   ASan/UBSan と TSan (ログの mutex・テストのクライアントのスレッドがある) で ctest。結果を記録したらビルドディレクトリを消す。
3. fuzz を接続: `option(MANUS_BUILD_FUZZERS OFF)`、ON なら Clang 必須と `-fsanitize=fuzzer-no-link` を manus_sanitize に足す。
   `add_subdirectory(fuzz)`。GCC で `manus.fuzz.*` (regression) を ctest に入れて通す。Clang で各入口を数十秒回す。
4. `spec/plan/cpp-port.md` の §3.3 と §6 を更新。報告を書く。

## (i) ローカルのビルドとテストの手順

前提: GCC 12 以上 (または Clang 18)、CMake 3.24 以上、Ninja、OpenSSL 3 の開発パッケージ、Python 3 (verify_binary)。
ネットワークがあれば Boost・nlohmann_json・Lorica は FetchContent が固定のハッシュ / commit で取る。
手元の写しを使うときは下の 3 つの `-D` を渡す (このコンテナでは共有キャッシュ)。

```sh
cd /home/user/Manus
export CCACHE_DIR=/home/user/.cpp-deps/ccache   # 任意
cmake -S native -B native/build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DMANUS_WARNINGS_AS_ERRORS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DLORICA_SOURCE_DIR=/home/user/Lorica \
  -DFETCHCONTENT_SOURCE_DIR_BOOST=/home/user/.cpp-deps/src/boost-1.87.0 \
  -DFETCHCONTENT_SOURCE_DIR_JSON=/home/user/.cpp-deps/src/nlohmann_json/json \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache            # 任意
/home/user/.cpp-deps/bin/buildslot cmake --build native/build -j2
ctest --test-dir native/build --output-on-failure      # 期待: 15/15
python3 -I /home/user/Lorica/tools/verify_binary.py --readelf readelf native/build/bin/manus
```

option: `MANUS_HARDEN_EXTENDED` (既定 ON)、`MANUS_WARNINGS_AS_ERRORS` (既定 OFF)。`MANUS_SANITIZE` と `MANUS_BUILD_FUZZERS` は未実装 ((h) の 2・3)。
本社 (Windows): `pwsh -File native/tests/tools/honsha-check.ps1 [-Smoke]` (前回の job で用意。WIP を仕上げるまでは Windows のビルドが通らない)。
