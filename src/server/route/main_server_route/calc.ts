/* コストと信号まの待ち時間を計算するAPI */

import { Hono } from 'hono';
import { runUp44, runYen } from '@/lib/wasm-utils';

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
        ].join(' ');

        const walkingSpeed = parseFloat(walkingSpeedStr || '80');
        const startNode = param1;
        const endNode = param2;

        // Run up44.wasm once to generate the cost file
        try {
            await runUp44([
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
            ]);
        } catch (err: any) {
            console.error(`[up44.wasm実行エラー] ${err.message}`);
            return c.json({ error: 'up44.wasmの実行に失敗しました' }, 500);
        }

        const startNodeInt = parseInt(startNode || '0', 10);
        const endNodeInt = parseInt(endNode || '0', 10);

        if (isNaN(startNodeInt) || isNaN(endNodeInt)) {
            return c.json({ error: 'Invalid start or end node' }, 400);
        }

        // Use WASM for complete calculation
        const startTime = Date.now();
        console.log(`[WASM計算] 期待値計算からイェンのアルゴリズムまでWASMで実行中...`);
        const yenStartTime = Date.now();

        try {
            // yen.wasmを実行
            const cProgramOutput = await runYen(startNodeInt, endNodeInt, walkingSpeed);

            const yenTime = Date.now() - yenStartTime;
            console.log(`[WASM計算完了] ${(yenTime / 1000).toFixed(2)}秒`);

            // JSONをパース
            const top5Routes = JSON.parse(cProgramOutput);

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
            console.error(`[WASM実行エラー詳細] ${cErr.stderr || cErr.stdout || ''}`);
            return c.json({ error: 'WASMプログラムの実行に失敗しました' }, 500);
        }
    } catch (err: any) {
        console.error(`実行エラー詳細: ${err.message}`);
        return c.json({ error: err.message, stderr: err.stderr, stdout: err.stdout }, 500);
    }
});

export default calc;
