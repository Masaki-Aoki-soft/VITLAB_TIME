/* メインAPI */

import { Hono } from 'hono';
import { cors } from 'hono/cors';
import { logger } from 'hono/logger';
import { csrf } from 'hono/csrf';
import { main_server_route } from './route/main_server_route';

const app = new Hono();

app.use('*', logger());
app.use('*', cors());
app.use('*', csrf());

export const route = app.route('/api/main_server_route', main_server_route);

export type AppType = typeof route;
export default app;
