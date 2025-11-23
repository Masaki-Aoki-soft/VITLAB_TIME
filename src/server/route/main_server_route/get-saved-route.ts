import { Hono } from 'hono';
import fs from 'fs';
import path from 'path';

const get_saved_data = new Hono().get('/getSavedRoute', async (c) => {
    const fileName = c.req.query('fileName');

    if (!fileName) {
        return c.json({ error: 'File name is required' }, 400);
    }

    const filePath = path.join(process.cwd(), 'saving_route', fileName);
    if (fs.existsSync(filePath)) {
        const data = fs.readFileSync(filePath, 'utf8');
        return c.text(data, 200, {
            'Content-Type': 'text/csv; charset=utf-8',
        });
    } else {
        return c.json({ error: 'File not found' }, 404);
    }
});

export default get_saved_data;
