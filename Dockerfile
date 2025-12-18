# マルチステージビルドを使用して最適化（Node.js使用）

# ステージ1: 依存関係のインストールとビルド
FROM node:18-alpine AS builder

# Cコンパイラとビルドツールをインストール（Cプログラムのコンパイル用）
RUN apk add --no-cache build-base gcc musl-dev

# 作業ディレクトリを設定
WORKDIR /app

# package.jsonとpackage-lock.jsonをコピー
COPY package*.json ./

# 依存関係をインストール（npmを使用）
RUN npm install --legacy-peer-deps

# プロジェクトファイルをコピー
COPY . .

# C言語のソースコードをコンパイルして実行ファイルを作成
RUN gcc spfa.c -o spfa21 && \
    gcc user_preference_speed.c -o up44 -lm && \
    gcc yens_algorithm.c -o yens_algorithm -lm -std=c99 && \
    gcc calculate_wait_time.c -o signal -lm -std=c99 && \
    chmod +x spfa21 up44 yens_algorithm signal

# Next.jsアプリケーションをビルド
RUN npm run build

# ステージ2: 本番用イメージ
FROM node:18-alpine AS runner

# セキュリティのため、非rootユーザーを作成
RUN addgroup --system --gid 1001 nodejs && \
    adduser --system --uid 1001 nextjs

# Cプログラムの実行に必要なライブラリをインストール
RUN apk add --no-cache libc6-compat

WORKDIR /app

# 本番環境用の環境変数を設定
ENV NODE_ENV=production
ENV NEXT_TELEMETRY_DISABLED=1

# Next.jsのビルド成果物をコピー
# standaloneモードでは、.next/standaloneに必要なファイルがすべて含まれます
COPY --from=builder --chown=nextjs:nodejs /app/.next/standalone ./
COPY --from=builder --chown=nextjs:nodejs /app/.next/static ./.next/static
# publicディレクトリを作成してからコピー（空でもOK）
RUN mkdir -p ./public
COPY --from=builder --chown=nextjs:nodejs /app/public ./public

# Cプログラムの実行ファイルをコピー
COPY --from=builder --chown=nextjs:nodejs /app/spfa21 /app/up44 /app/yens_algorithm /app/signal ./

# データファイルをコピー（必要に応じて）
COPY --from=builder --chown=nextjs:nodejs /app/oomiya_line ./oomiya_line
COPY --from=builder --chown=nextjs:nodejs /app/oomiya_point ./oomiya_point
COPY --from=builder --chown=nextjs:nodejs /app/*.csv ./
COPY --from=builder --chown=nextjs:nodejs /app/*.geojson ./
COPY --from=builder --chown=nextjs:nodejs /app/*.txt ./
COPY --from=builder --chown=nextjs:nodejs /app/saving_route ./saving_route
COPY --from=builder --chown=nextjs:nodejs /app/signal_inf.csv ./
COPY --from=builder --chown=nextjs:nodejs /app/194-195_green ./194-195_green
COPY --from=builder --chown=nextjs:nodejs /app/194-195_red ./194-195_red

# ユーザーを変更
USER nextjs

# ポート3000を公開（Next.jsのデフォルトポート）
EXPOSE 3000

ENV PORT=3000
ENV HOSTNAME="0.0.0.0"

# Next.jsサーバーを起動
CMD ["node", "server.js"]
