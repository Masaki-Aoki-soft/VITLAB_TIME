import type { Metadata } from 'next';
import { Inter } from 'next/font/google';
import { NextFont } from 'next/dist/compiled/@next/font';
import { ReactNode, Suspense } from 'react';
import './styles/globals.css';
import { ToasterContext } from './context/Toastcontest';

export const metadata: Metadata = {
    title: '経路探索マップ',
    description: '経路探索マップアプリケーション',
};

const inter: NextFont = Inter({ subsets: ['latin'] });

export default function RootLayout({ children }: { children: ReactNode }) {
    return (
        <html lang="ja">
            <Suspense>
                <head>
                    <link
                        rel="stylesheet"
                        href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css"
                    />
                    <link
                        rel="stylesheet"
                        href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"
                        integrity="sha256-p4NxAoJBhIIN+hmNHrzRCf9tD/miZyoHS5obTRR9BMY="
                        crossOrigin=""
                    />
                </head>
                <body className={`${inter.className} text-amber-200`}>
                    {children}
                    <ToasterContext />
                </body>
            </Suspense>
        </html>
    );
}
