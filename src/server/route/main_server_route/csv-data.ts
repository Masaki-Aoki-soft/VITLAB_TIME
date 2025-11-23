import { Hono } from 'hono';
import fs from 'fs';
import path from 'path';

const csv_data = new Hono().get('/csvData', async (c) => {
    try {
        const csvPath = path.join(process.cwd(), 'oomiya_route_inf_4.csv');
        const csvData = fs.readFileSync(csvPath, 'utf8');
        return c.text(csvData, 200, {
            'Content-Type': 'text/csv; charset=utf-8',
        });
    } catch (err: any) {
        return c.json({ error: `CSV Error: ${err.message}` }, 500);
    }
});

export default csv_data;
