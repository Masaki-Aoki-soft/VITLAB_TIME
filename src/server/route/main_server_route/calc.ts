/* コストと信号まの待ち時間を計算するAPI */

import { Hono } from 'hono';
import { execSync } from 'child_process';

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

        // Run up44 once to generate the cost file
        const projectRoot = process.cwd();
        execSync(`./up44 ${base_args}`, { encoding: 'utf8', cwd: projectRoot });

        const startNodeInt = parseInt(startNode || '0', 10);
        const endNodeInt = parseInt(endNode || '0', 10);

        if (isNaN(startNodeInt) || isNaN(endNodeInt)) {
            return c.json({ error: 'Invalid start or end node' }, 400);
        }

        // Use C program for complete calculation
        const startTime = Date.now();
        console.log(`[C言語計算] 期待値計算からイェンのアルゴリズムまでC言語で実行中...`);
        const yenStartTime = Date.now();

        try {
            // Cプログラムを実行
            const cProgramOutput = execSync(
                `./yens_algorithm ${startNodeInt} ${endNodeInt} ${walkingSpeed}`,
                { encoding: 'utf8', cwd: projectRoot, maxBuffer: 10 * 1024 * 1024 }
            );

            const yenTime = Date.now() - yenStartTime;
            console.log(`[C言語計算完了] ${(yenTime / 1000).toFixed(2)}秒`);

            // JSONをパース
            const rawRoutes = JSON.parse(cProgramOutput);

            if (!Array.isArray(rawRoutes) || rawRoutes.length === 0) {
                return c.json([]);
            }

            // Cプログラムの出力をアプリ用の型に正規化
            // - C側の totalTime は秒なので、分に変換して扱う
            // - totalWaitTime は C 側では未定義なので 0 をデフォルトにする
            const top5Routes = rawRoutes.map((route: any) => {
                const totalDistance = Number(route.totalDistance ?? 0);
                const totalTimeSeconds = Number(route.totalTime ?? 0);
                const totalWaitTime = Number(route.totalWaitTime ?? 0);

                return {
                    ...route,
                    totalDistance,
                    // アプリ全体では「分」を使うのでここで秒→分に変換
                    totalTime: totalTimeSeconds / 60,
                    totalWaitTime,
                };
            });

            const totalTime = Date.now() - startTime;
            console.log(`[最終結果] C言語計算: ${top5Routes.length}件の経路を発見`);
            console.log(`[最終結果] 上位${top5Routes.length}件の経路を選択`);
            top5Routes.forEach((route: any, index: number) => {
                const totalTimeMinutes =
                    typeof route.totalTime === 'number' && isFinite(route.totalTime)
                        ? route.totalTime
                        : 0;
                const totalDistance =
                    typeof route.totalDistance === 'number' && isFinite(route.totalDistance)
                        ? route.totalDistance
                        : 0;
                const totalWaitTimeMinutes =
                    typeof route.totalWaitTime === 'number' && isFinite(route.totalWaitTime)
                        ? route.totalWaitTime
                        : 0;

                console.log(
                    `[経路${index + 1}] 総推定時間: ${totalTimeMinutes.toFixed(
                        2
                    )}分, 距離: ${totalDistance.toFixed(
                        2
                    )}m, 待ち時間: ${totalWaitTimeMinutes.toFixed(2)}分`
                );
            });
            console.log(
                `[総処理時間] ${(totalTime / 1000).toFixed(2)}秒（${(totalTime / 60000).toFixed(
                    2
                )}分）`
            );
            console.log(`[処理時間内訳] C言語計算: ${(yenTime / 1000).toFixed(2)}秒`);

            return c.json(top5Routes);
        } catch (cErr: any) {
            console.error(`[C言語実行エラー] ${cErr.message}`);
            console.error(`[C言語実行エラー詳細] ${cErr.stderr || cErr.stdout || ''}`);
            return c.json({ error: 'C言語プログラムの実行に失敗しました' }, 500);
        }
    } catch (err: any) {
        console.error(`実行エラー詳細: ${err.message}`);
        return c.json({ error: err.message, stderr: err.stderr, stdout: err.stdout }, 500);
    }
});

export default calc;
