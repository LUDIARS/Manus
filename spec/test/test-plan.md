# Manus テスト計画

## 単体 (vitest, `npm test`)

- `ws/protocol` — メッセージ schema の受理/拒否 (不正 type / 範囲外値 / 余剰フィールド)。
- `inject/jis-vk-map` — キー名→VK の網羅 (f1..f12 / 矢印 / JIS 固有キー) と重複 VK なし。
- `inject/keyboard` — text→ヘルパ op 変換 (`\n`→enter, `\t`→tab, サロゲートペア分割しない)。
- `auth/pairing` — PIN hash 往復 / トークン発行→検証 / 期限切れ拒否 / 改竄拒否。
- `profiles` (実装後) — resolve の優先順位 / url regex / fallback。

## 実経路 (手動 — セッションからのサービス動作テストは禁止のため人間 or Excubitor 経由)

1. Excubitor から manus 起動 → `/health` が ok + helper up。
2. iPhone 実機で PIN ペアリング → キーボードペインから日本語文字列がメモ帳に入る。
3. 遅延 HUD の RTT を Wi-Fi 実環境で計測 (MVP の判断材料)。
4. タッチパッドでカーソル移動 / クリック / スクロール。
5. ヘルパ kill → 自動再起動 → 注入継続。

## ヘルパ単体スモーク (セッション内可)

`node scripts/helper-smoke.mjs` — csc ビルド + spawn + `ping` op の応答のみ確認
(キー注入はしない。セッション環境へ入力が飛ぶため)。
