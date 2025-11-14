/** @type {import('tailwindcss').Config} */
module.exports = {
    content: [
        './*.html', // 静的HTMLならここを指定
        './src/**/*.{js,ts,jsx,tsx}', // React/Next.jsを使う場合
    ],
    theme: {
        extend: {
            colors: {
                primary: {
                    50: '#f0f4ff',
                    100: '#e0e9ff',
                    500: '#667eea',
                    600: '#5a67d8',
                    700: '#4c51bf',
                },
            },
            backdropBlur: {
                xs: '2px',
            },
        },
    },
    plugins: [],
};
