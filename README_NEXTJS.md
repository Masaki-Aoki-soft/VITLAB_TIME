# Next.js移行版 - 経路探索マップアプリケーション

このプロジェクトは、元のHTML + Node.jsサーバー構成からNext.jsに移行したバージョンです。

## 主な変更点

### アーキテクチャ
- **フロントエンド**: HTML + Vanilla JavaScript → Next.js (React) + TypeScript
- **バックエンド**: Node.js HTTPサーバー → Next.js API Routes
- **スタイリング**: Tailwind CSS（継続使用）

### ディレクトリ構造
```
├── app/                    # Next.js App Router
│   ├── api/                # API Routes（バックエンド）
│   ├── globals.css         # グローバルスタイル
│   ├── layout.tsx          # ルートレイアウト
│   └── page.tsx            # メインページ
├── components/             # Reactコンポーネント
│   ├── MapComponent.tsx    # Leafletマップコンポーネント
│   ├── LoadingOverlay.tsx # ローディング表示
│   ├── RouteInfo.tsx      # 経路情報表示
│   ├── Toast.tsx          # トースト通知
│   └── WeightSlider.tsx   # 重み設定スライダー
├── lib/                    # ユーティリティ関数
│   ├── types.ts           # TypeScript型定義
│   └── utils.ts           # ヘルパー関数
└── public/                 # 静的ファイル（必要に応じて）
```

## セットアップ

### 1. 依存関係のインストール

```bash
npm install
```

### 2. 開発サーバーの起動

```bash
npm run dev
```

ブラウザで [http://localhost:3000](http://localhost:3000) を開きます。

### 3. 本番ビルド

```bash
npm run build
npm start
```

## API Routes

既存のバックエンド機能は以下のAPI Routesに移行されています：

- `POST /api/calc` - 経路計算
- `POST /api/calculate-wait-time` - 信号待ち時間計算
- `GET /api/csv-data` - CSVデータ取得
- `GET /api/list-saved-routes` - 保存済みルート一覧
- `GET /api/get-saved-route` - 保存済みルート取得
- `POST /api/save-route` - ルート保存
- `GET /api/static/[...path]` - 静的ファイル配信（GeoJSON、CSVなど）

## 静的ファイル

静的ファイル（GeoJSON、CSVなど）は、プロジェクトルートに配置されたままです。
Next.jsのAPI Route (`/api/static/[...path]`) を通じて配信されます。

例：
- `/api/static/oomiya_line/1-2.geojson`
- `/api/static/oomiya_point/1.geojson`
- `/api/static/oomiya_route_inf_4.csv`

## 既存の機能

以下の機能はすべてNext.js版でも利用可能です：

- ✅ 経路探索（Yen's algorithm）
- ✅ 重み設定（距離、勾配、歩道、信号など）
- ✅ マップ表示（Leaflet）
- ✅ 経路保存・読み込み
- ✅ 信号待ち時間計算
- ✅ 複数経路の表示
- ✅ データ表示（歩道、信号、道路幅など）

## 注意事項

1. **Cプログラムの実行**: 既存のCプログラム（`yens_algorithm`、`up44`、`signal`など）は引き続き使用されます。これらの実行ファイルがプロジェクトルートに存在することを確認してください。

2. **ファイルパス**: 静的ファイルへのアクセスは `/api/static/` 経由に変更されています。

3. **環境変数**: 必要に応じて `.env.local` ファイルを作成して環境変数を設定できます。

## トラブルシューティング

### マップが表示されない
- ブラウザのコンソールでエラーを確認してください
- LeafletのCSSが正しく読み込まれているか確認してください

### APIエラー
- Cプログラムの実行ファイルが存在するか確認してください
- ファイルパーミッションを確認してください

### 静的ファイルが読み込めない
- `/api/static/` 経由でアクセスしているか確認してください
- ファイルがプロジェクトルートに存在するか確認してください

## 旧バージョンとの比較

### 旧バージョン（HTML + Node.js）
```bash
npm run legacy:server  # html_server_ver6.1.js を実行
```

### 新バージョン（Next.js）
```bash
npm run dev  # Next.js開発サーバーを起動
```

## 技術スタック

- **フレームワーク**: Next.js 14
- **言語**: TypeScript
- **UIライブラリ**: React 18
- **マップ**: Leaflet
- **スタイリング**: Tailwind CSS
- **データ処理**: PapaParse, Turf.js

## ライセンス

ISC


