# WASMビルドガイド

このプロジェクトでは、C言語のプログラムをWASM形式にコンパイルして使用しています。

## 前提条件

1. **Emscriptenのインストール**
   - Emscriptenをインストールする必要があります
   - インストール方法: https://emscripten.org/docs/getting_started/downloads.html
   - インストール後、`emcc`コマンドがPATHに含まれていることを確認してください

## ビルド方法

### Windows
```bash
npm run build:wasm:win
```
または
```bash
build-wasm.bat
```

### Linux/Mac
```bash
npm run build:wasm:unix
```
または
```bash
bash build-wasm.sh
```

### クロスプラットフォーム（Node.js）
```bash
npm run build:wasm
```
または
```bash
node build-wasm.js
```

## 生成されるファイル

ビルドが成功すると、ルートディレクトリに以下のスタンドアロンWASMファイルが生成されます：

- `user_preference_ver4.4.wasm` - ユーザー好みの重み計算用スタンドアロンWASMモジュール
- `yens_algorithm.wasm` - イェンのアルゴリズム用スタンドアロンWASMモジュール
- `calculate_wait_time.wasm` - 信号待ち時間計算用スタンドアロンWASMモジュール

**注意**: スタンドアロンWASMファイルは、JavaScriptラッパーなしで純粋なWASMバイナリです。

## 使用方法

ビルド後、アプリケーションは自動的にWASMモジュールを使用します。

以前は`child_process`でCプログラムを実行していましたが、現在はWASMモジュールを使用するように変更されています。

## トラブルシューティング

### Emscriptenが見つからない
- `emcc --version`を実行して、Emscriptenが正しくインストールされているか確認してください
- Emscriptenの環境変数が正しく設定されているか確認してください

### WASMモジュールが見つからない
- ビルドが完了しているか確認してください
- ルートディレクトリに`.wasm`と`.js`ファイルが存在するか確認してください

### メモリ不足エラー
- ビルドスクリプトの`INITIAL_MEMORY`パラメータを調整してください
- より大きな値が必要な場合は、`build-wasm.js`を編集してください

## 注意事項

- WASMモジュールは初回読み込み時にメモリを確保します
- 大きなデータセットを使用する場合は、メモリ設定を調整する必要があるかもしれません
- ファイルシステムへのアクセスは、Emscriptenの仮想ファイルシステム（FS）を使用します

