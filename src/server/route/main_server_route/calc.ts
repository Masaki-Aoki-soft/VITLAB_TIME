/* コストと信号まの待ち時間を計算するAPI */

import { Hono } from 'hono';
import fs from 'fs';
import path from 'path';
import { loadWASMModule, writeFileToWASMFS, readFileFromWASMFS, captureStdout } from '../../../lib/wasm-loader';

const calc = new Hono().post('/calc', async (c) => {
    try {
        const body = await c.req.json();
        const {
            weight0,
            weight1,
            weight2,
            weight3,
            weight4,
            weight5,
            weight6,
            weight7,
            weight8,
            weight9,
            weight10,
            weight11,
            weight12,
            param1,
            param2,
            walkingSpeed: walkingSpeedStr,
        } = body;

        const walkingSpeed = parseFloat(walkingSpeedStr || '80');
        const startNode = param1;
        const endNode = param2;

        const startNodeInt = parseInt(startNode || '0', 10);
        const endNodeInt = parseInt(endNode || '0', 10);

        if (isNaN(startNodeInt) || isNaN(endNodeInt)) {
            return c.json({ error: 'Invalid start or end node' }, 400);
        }

        const projectRoot = process.cwd();

        // WASMモジュールを読み込む
        console.log('[WASM] user_preference_ver4.4モジュールを読み込み中...');
        const up44Module = await loadWASMModule('user_preference_ver4.4');
        
        // 必要なファイルをWASMの仮想ファイルシステムにコピー
        const inputFile = path.join(projectRoot, 'oomiya_route_inf_4.csv');
        if (!fs.existsSync(inputFile)) {
            return c.json({ error: 'oomiya_route_inf_4.csv not found' }, 500);
        }
        const inputData = fs.readFileSync(inputFile, 'utf8');
        writeFileToWASMFS(up44Module, 'oomiya_route_inf_4.csv', inputData);

        // user_preference_ver4.4を実行
        const base_args = [
            weight0,
            weight1,
            weight2,
            weight3,
            weight4,
            weight5,
            weight6,
            weight7,
            weight8,
            weight9,
            weight10,
            weight11,
            weight12,
            param1,
            param2,
        ];
        
        console.log('[WASM] user_preference_ver4.4を実行中...');
        let stdout = '';
        const originalPrint = up44Module.print;
        up44Module.print = (text: string) => {
            stdout += text;
        };
        
        try {
            up44Module.callMain(['user_preference_ver4.4', ...base_args.map(String)]);
        } finally {
            up44Module.print = originalPrint;
        }

        // result.csvをWASMの仮想ファイルシステムから読み込む
        const resultCsv = readFileFromWASMFS(up44Module, 'result.csv');
        // 実際のファイルシステムにも保存（後続の処理のため）
        fs.writeFileSync(path.join(projectRoot, 'result.csv'), resultCsv);

        // Use WASM for complete calculation
        const startTime = Date.now();
        console.log(`[WASM計算] 期待値計算からイェンのアルゴリズムまでWASMで実行中...`);
        const yenStartTime = Date.now();

        try {
            // yens_algorithm WASMモジュールを読み込む
            console.log('[WASM] yens_algorithmモジュールを読み込み中...');
            const yenModule = await loadWASMModule('yens_algorithm');
            
            // 必要なファイルをWASMの仮想ファイルシステムにコピー
            writeFileToWASMFS(yenModule, 'result.csv', resultCsv);
            
            const routeDataFile = path.join(projectRoot, 'oomiya_route_inf_4.csv');
            const routeData = fs.readFileSync(routeDataFile, 'utf8');
            writeFileToWASMFS(yenModule, 'oomiya_route_inf_4.csv', routeData);
            
            const signalDataFile = path.join(projectRoot, 'signal_inf.csv');
            if (fs.existsSync(signalDataFile)) {
                const signalData = fs.readFileSync(signalDataFile, 'utf8');
                writeFileToWASMFS(yenModule, 'signal_inf.csv', signalData);
            }

            // yens_algorithmを実行
            stdout = '';
            const originalYenPrint = yenModule.print;
            yenModule.print = (text: string) => {
                stdout += text;
            };
            
            try {
                yenModule.callMain(['yens_algorithm', String(startNodeInt), String(endNodeInt), String(walkingSpeed)]);
            } finally {
                yenModule.print = originalYenPrint;
            }

            const yenTime = Date.now() - yenStartTime;
            console.log(`[WASM計算完了] ${(yenTime / 1000).toFixed(2)}秒`);

            // JSONをパース（標準出力から取得）
            // 標準出力からJSON部分を抽出
            const jsonMatch = stdout.match(/\[[\s\S]*\]/);
            if (!jsonMatch) {
                throw new Error('JSON output not found in WASM output');
            }
            
            const top5Routes = JSON.parse(jsonMatch[0]);

            if (!Array.isArray(top5Routes) || top5Routes.length === 0) {
                return c.json([]);
            }

            const totalTime = Date.now() - startTime;
            console.log(`[最終結果] WASM計算: ${top5Routes.length}件の経路を発見`);
            console.log(`[最終結果] 上位${top5Routes.length}件の経路を選択`);
            top5Routes.forEach((route: any, index: number) => {
                console.log(
                    `[経路${index + 1}] 総推定時間: ${route.totalTime.toFixed(
                        2
                    )}分, 距離: ${route.totalDistance.toFixed(
                        2
                    )}m, 待ち時間: ${route.totalWaitTime.toFixed(2)}分`
                );
            });
            console.log(
                `[総処理時間] ${(totalTime / 1000).toFixed(2)}秒（${(totalTime / 60000).toFixed(
                    2
                )}分）`
            );
            console.log(`[処理時間内訳] WASM計算: ${(yenTime / 1000).toFixed(2)}秒`);

            return c.json(top5Routes);
        } catch (cErr: any) {
            console.error(`[WASM実行エラー] ${cErr.message}`);
            console.error(`[WASM実行エラー詳細] ${cErr.stack || ''}`);
            return c.json({ error: 'WASMプログラムの実行に失敗しました' }, 500);
        }
    } catch (err: any) {
        console.error(`実行エラー詳細: ${err.message}`);
        return c.json({ error: err.message, stack: err.stack }, 500);
    }
});

export default calc;
