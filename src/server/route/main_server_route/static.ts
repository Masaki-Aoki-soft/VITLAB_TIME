import { Hono } from 'hono';
import fs from 'fs';
import path from 'path';
import mime from 'mime-types';

const static_route = new Hono().get('/*', async (c) => {
    try {
        // Honoのルーティングでは、マウントされたパスの後の部分を取得
        // /api/main_server_route/static/... の形式で来るので、/api/main_server_route/static/ を削除
        const routePath = c.req.path.replace(/^\/api\/main_server_route\/static\/?/, '');
        const pathSegments = routePath.split('/').filter(Boolean);
        const filePath = path.join(process.cwd(), ...pathSegments);
        
        // デバッグログ
        console.log(`[静的ファイル] リクエストパス: ${c.req.path}, routePath: ${routePath}, filePath: ${filePath}`);

        // セキュリティチェック：プロジェクトルート外へのアクセスを防ぐ
        const resolvedPath = path.resolve(filePath);
        const projectRoot = path.resolve(process.cwd());
        if (!resolvedPath.startsWith(projectRoot)) {
            return c.json({ error: 'Forbidden' }, 403);
        }

        if (!fs.existsSync(filePath)) {
            console.error(`[静的ファイル] ファイルが見つかりません: ${filePath}`);
            console.error(`[静的ファイル] process.cwd(): ${process.cwd()}`);
            console.error(`[静的ファイル] 解決されたパス: ${resolvedPath}`);
            return c.json({ error: 'Not Found', filePath, resolvedPath, cwd: process.cwd() }, 404);
        }

        const stats = fs.statSync(filePath);
        if (stats.isDirectory()) {
            return c.json({ error: 'Not Found' }, 404);
        }

        const fileContent = fs.readFileSync(filePath);
        let mimeType = mime.lookup(filePath) || 'application/octet-stream';
        
        // テキストファイルの場合は明示的にtext/plainを設定
        if (filePath.endsWith('.txt')) {
            mimeType = 'text/plain; charset=utf-8';
        }

        return c.body(fileContent, 200, {
            'Content-Type': mimeType,
            'Cache-Control': 'public, max-age=3600',
        });
    } catch (err: any) {
        return c.json({ error: err.message }, 500);
    }
});

export default static_route;
