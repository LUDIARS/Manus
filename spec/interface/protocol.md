# Manus WebSocket / HTTP プロトコル

## HTTP

| Method/Path | 認可 | 内容 |
|---|---|---|
| `GET /health` | 不要 | `{ ok, service:"manus", helper:"up"\|"down", foreground:{exe,title,url?}\|null }` |
| `POST /api/pair` | 不要 | body `{ pin }` → `{ token }` (不一致 401、5 回失敗で 60 秒ロック) |
| `GET /api/profiles` | Bearer | 全プロファイル (既定 + ユーザ、同 id はユーザ優先) |
| `GET /api/profiles/resolve?exe=<exe>&url=<url>` | Bearer | 解決済みプロファイル 1 件 (無ければ既定 fallback プロファイル) |
| `PUT /api/profiles/<id>` | Bearer | ユーザプロファイル保存 (`data/profiles/<id>.json`、schema 検証) |
| `DELETE /api/profiles/<id>` | Bearer | ユーザプロファイル削除 (既定は消えず復帰) |
| `GET /` `/keyboard` `/touchpad` `/shortcuts` `/editor` | 不要 (操作は Bearer/WS で認可) | 静的 UI |

## WebSocket `/ws?token=<token>`

トークン無効なら即 close (code 4401)。メッセージはすべて JSON 1 行。

### client → server

| type | フィールド | 意味 |
|---|---|---|
| `text` | `text: string` | 文字列を Unicode 注入 (`\n`→Enter、`\t`→Tab) |
| `key` | `key: JisKeyName, modifiers?: ("ctrl"\|"shift"\|"alt"\|"win")[], action?: "press"\|"down"\|"up"` | JIS 108 キー名で特殊キー / ショートカット |
| `mouse.move` | `dx, dy: number` | 相対移動 (整数 px) |
| `mouse.button` | `button: "left"\|"right"\|"middle", action: "down"\|"up"\|"click"` | |
| `mouse.wheel` | `dx?, dy?: number` | ホイール (120 = 1 ノッチ) |
| `shortcut` | `profileId: string, buttonId: string` | プロファイルのボタン action を実行 |
| `ping` | | keepalive |

`pad.*` (ゲームパッド) は将来予約 — 現状は `error: pad_deferred` を返す。

### server → client

| type | 内容 |
|---|---|
| `foreground` | `{ type:"foreground", exe, title, url? }` — 最前面変化時に全認可接続へ push (接続直後にも現在値を 1 回送る) |
| `ack` | `{ type:"ack", seq }` (client が `seq` を付けた場合のみ) |
| `error` | `{ type:"error", code, message, seq? }` — `bad_message` / `pad_deferred` / `helper_down` / `unknown_profile` 等 |
| `pong` | ping 応答 |

## プロファイル schema (profiles/*.json)

```jsonc
{
  "id": "blender",
  "name": "Blender",
  "match": { "exe": ["blender.exe"] },          // url: ["regex", ...] も可 (ブラウザ用)
  "priority": 10,                                // 複数一致時は大きい方
  "grid": { "cols": 4 },
  "buttons": [
    { "id": "save", "label": "保存", "row": 0, "col": 0,
      "action": { "kind": "keys", "key": "s", "modifiers": ["ctrl"] } },
    { "id": "orbit", "label": "オービット", "row": 0, "col": 1,
      "action": { "kind": "mouseButton", "button": "middle", "mode": "hold" } },  // 3D マウス的操作: 保持中 touchpad が回転に
    { "id": "zoom+", "label": "ズーム+", "row": 0, "col": 2,
      "action": { "kind": "wheel", "dy": 120 } },
    { "id": "greet", "label": "定型文", "row": 0, "col": 3,
      "action": { "kind": "text", "text": "お疲れ様です。" } },
    { "id": "combo", "label": "マクロ", "row": 1, "col": 0,
      "action": { "kind": "macro", "steps": [ { "kind": "keys", "key": "esc" }, { "kind": "text", "text": "gz" } ] } }
  ]
}
```

`key` の語彙は `src/inject/jis-vk-map.ts` が正本 (enter/esc/tab/f1..f12/矢印/home/end/
pgup/pgdn/del/bs/space/半角全角/変換/無変換/かな/英数 + 英数字/JIS 記号)。
