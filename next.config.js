/** @type {import('next').NextConfig} */
const nextConfig = {
  reactStrictMode: true,
  // Docker用にstandaloneモードを有効化
  output: 'standalone',
  // 静的ファイルの配信設定
  async headers() {
    return [
      {
        source: '/:path*',
        headers: [
          {
            key: 'Access-Control-Allow-Origin',
            value: '*',
          },
          {
            key: 'Access-Control-Allow-Methods',
            value: 'GET, POST, OPTIONS',
          },
          {
            key: 'Access-Control-Allow-Headers',
            value: 'Content-Type',
          },
        ],
      },
    ];
  },
};

module.exports = nextConfig;

