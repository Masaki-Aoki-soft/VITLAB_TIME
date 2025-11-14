# ベースイメージとしてUbuntu 20.04を使用
FROM ubuntu:20.04

# 環境変数を設定
ENV DEBIAN_FRONTEND=noninteractive

# Cコンパイラ(gcc)とNode.jsをインストール
RUN apt-get update && \
    apt-get install -y build-essential curl && \
    curl -fsSL https://deb.nodesource.com/setup_18.x | bash - && \
    apt-get install -y nodejs && \
    rm -rf /var/lib/apt/lists/*

# アプリケーションの作業ディレクトリを設定
WORKDIR /app

# package.jsonとpackage-lock.jsonをコピーし、依存関係をインストール
COPY package*.json ./
RUN npm install

# プロジェクトの全てのファイルをコンテナにコピー
COPY . .

# C言語のソースコードをコンパイルして実行ファイルを作成
# -lm オプションは、mathライブラリ（pow関数など）をリンクするために必要
RUN gcc spfa.c -o spfa21
RUN gcc user_preference_speed.c -o up44 -lm
RUN gcc yens_algorithm.c -o yens_algorithm -lm -std=c99

# 作成した実行ファイルに実行権限を付与
RUN chmod +x spfa21 up44 yens_algorithm

# ポート8081を公開
EXPOSE 8081

# サーバーを起動
CMD ["node", "html_server_ver6.1.js"]