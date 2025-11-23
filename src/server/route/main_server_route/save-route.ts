import { Hono } from 'hono';
import fs from 'fs';
import path from 'path';

const save_route = new Hono().post('/saveRoute', async (c) => {
    try {
        const { fileName, csvContent, overwrite } = await c.req.json();

        let finalFileName = fileName;
        if (!finalFileName.toLowerCase().endsWith('.csv')) {
            finalFileName += '.csv';
        }

        const saveDir = path.join(process.cwd(), 'saving_route');
        const filePath = path.join(saveDir, finalFileName);

        if (!fs.existsSync(saveDir)) {
            fs.mkdirSync(saveDir, { recursive: true });
        }

        if (fs.existsSync(filePath) && !overwrite) {
            return c.json({ status: 'exists', finalFileName });
        } else {
            fs.writeFileSync(filePath, csvContent, 'utf8');
            return c.json({ status: 'success', finalFileName });
        }
    } catch (err: any) {
        return c.json({ status: 'error', message: err.message }, 500);
    }
});

export default save_route;
