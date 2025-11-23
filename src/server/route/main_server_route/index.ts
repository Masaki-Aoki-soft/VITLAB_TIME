/* 統合用API */

import { Hono } from 'hono';
import calcRoute from './calc';
import calculateWaitTimeRoute from './calculate-wait-time';
import csvDataRoute from './csv-data';
import getSavedRouteRoute from './get-saved-route';
import listSavedRoutesRoute from './list-saved-routes';
import saveRouteRoute from './save-route';
import staticRoute from './static';

export const main_server_route = new Hono()
    .route('/', calcRoute)
    .route('/', calculateWaitTimeRoute)
    .route('/', csvDataRoute)
    .route('/', getSavedRouteRoute)
    .route('/', listSavedRoutesRoute)
    .route('/', saveRouteRoute)
    .route('/static', staticRoute);
