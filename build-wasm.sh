#!/bin/bash

# EmscriptenでCプログラムをスタンドアロンWASMにコンパイルするスクリプト

# Emscriptenがインストールされているか確認
if ! command -v emcc &> /dev/null; then
    echo "Error: Emscripten (emcc) is not installed or not in PATH"
    echo "Please install Emscripten: https://emscripten.org/docs/getting_started/downloads.html"
    exit 1
fi

echo "Building standalone WASM modules..."

# user_preference_ver4.4.c をスタンドアロンWASMにコンパイル
echo "Building user_preference_ver4.4.wasm..."
emcc user_preference_ver4.4.c \
    -o user_preference_ver4.4.wasm \
    -s STANDALONE_WASM=1 \
    -s EXPORTED_FUNCTIONS='["_main","_malloc","_free"]' \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s INITIAL_MEMORY=16777216 \
    -O2

# yens_algorithm.c をスタンドアロンWASMにコンパイル
echo "Building yens_algorithm.wasm..."
emcc yens_algorithm.c \
    -o yens_algorithm.wasm \
    -s STANDALONE_WASM=1 \
    -s EXPORTED_FUNCTIONS='["_main","_malloc","_free"]' \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s INITIAL_MEMORY=33554432 \
    -O2

# calculate_wait_time.c をスタンドアロンWASMにコンパイル
echo "Building calculate_wait_time.wasm..."
emcc calculate_wait_time.c \
    -o calculate_wait_time.wasm \
    -s STANDALONE_WASM=1 \
    -s EXPORTED_FUNCTIONS='["_main","_malloc","_free"]' \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s INITIAL_MEMORY=16777216 \
    -O2

echo "Standalone WASM build complete!"

