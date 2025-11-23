import { Hono } from 'hono';
import fs from 'fs';
import path from 'path';

const list_saved_route = new Hono().get('/listSavedRoutes', async (c) => {
    const saveDir = path.join(process.cwd(), 'saving_route');
    if (!fs.existsSync(saveDir)) {
        return c.json([]);
    }

    try {
        const files = fs.readdirSync(saveDir);
        const csvFiles = files.filter((file) => file.toLowerCase().endsWith('.csv'));
        return c.json(csvFiles);
    } catch (err: any) {
        return c.json({ error: 'Failed to list files' }, 500);
    }
});

export default list_saved_route;
