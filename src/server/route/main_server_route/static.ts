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

        // セキュリティチェック：プロジェクトルート外へのアクセスを防ぐ
        const resolvedPath = path.resolve(filePath);
        const projectRoot = path.resolve(process.cwd());
        if (!resolvedPath.startsWith(projectRoot)) {
            return c.json({ error: 'Forbidden' }, 403);
        }

        if (!fs.existsSync(filePath)) {
            return c.json({ error: 'Not Found' }, 404);
        }

        const stats = fs.statSync(filePath);
        if (stats.isDirectory()) {
            return c.json({ error: 'Not Found' }, 404);
        }

        const fileContent = fs.readFileSync(filePath);
        const mimeType = mime.lookup(filePath) || 'application/octet-stream';

        return c.body(fileContent, 200, {
            'Content-Type': mimeType,
            'Cache-Control': 'public, max-age=3600',
        });
    } catch (err: any) {
        return c.json({ error: err.message }, 500);
    }
});

export default static_route;
