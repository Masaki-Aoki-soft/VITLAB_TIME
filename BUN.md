# Bunへの移行ガイド

このプロジェクトはnpmからBunに移行されました。

## Bunとは

Bunは、高速なJavaScriptランタイム、バンドラー、パッケージマネージャーです。
- **高速**: npmより10-100倍高速なパッケージインストール
- **互換性**: Node.jsと完全互換
- **統合**: ランタイム、バンドラー、パッケージマネージャーが統合

## セットアップ

### 1. Bunのインストール

```bash
# macOS/Linux
curl -fsSL https://bun.sh/install | bash

# Windows (PowerShell)
powershell -c "irm bun.sh/install.ps1 | iex"
```

### 2. 依存関係のインストール

```bash
bun install
```

### 3. 開発サーバーの起動

```bash
bun run dev
```

### 4. 本番ビルド

```bash
bun run build
bun run start
```

## Dockerでの使用

### 本番環境

```bash
docker-compose build
docker-compose up -d
```

### 開発環境

```bash
docker-compose -f docker-compose.dev.yml build
docker-compose -f docker-compose.dev.yml up
```

## 主な変更点

1. **package.json**: スクリプトが`bun run`を使用
2. **Dockerfile**: `oven/bun:1-alpine`イメージを使用
3. **依存関係管理**: `bun.lockb`ファイルを使用（`package-lock.json`の代わり）

## メリット

- **高速なインストール**: npmより大幅に高速
- **統合ツール**: ランタイム、バンドラー、パッケージマネージャーが統合
- **TypeScriptサポート**: ネイティブサポート
- **互換性**: Node.jsと完全互換

## トラブルシューティング

### bun.lockbが存在しない

```bash
# bun installを実行してlockファイルを生成
bun install
```

### 依存関係のエラー

```bash
# キャッシュをクリア
rm -rf node_modules bun.lockb
bun install
```

### Dockerビルドエラー

```bash
# キャッシュをクリアして再ビルド
docker-compose build --no-cache
```

