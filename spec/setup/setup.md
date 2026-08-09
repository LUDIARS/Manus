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

## iPhone 側

1. PC と同じ Wi-Fi に接続。
2. Safari で `http://<PCのIP>:5390/` を開く。
3. PIN を入力してペアリング (トークンは localStorage に 24h 保持)。
4. 共有シート → 「ホーム画面に追加」で全画面化 (任意)。

## ゲームパッド (後回し・未実装)

実装時は ViGEmBus ドライバ + `vigemclient` が必要になる予定。現段階では不要。
