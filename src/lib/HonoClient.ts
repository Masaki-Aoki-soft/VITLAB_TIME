/* HonoRPCのクライアントライブラリ */

import { hc } from 'hono/client';
import { type AppType } from '@/server';

export const client = hc<AppType>(
    typeof window !== 'undefined' ? '' : process.env.NEXT_PUBLIC_APP_BASE_URL || ''
);
