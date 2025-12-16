/* コストと信号まの待ち時間を計算するAPI */

import { Hono } from 'hono';
import { execSync } from 'child_process';
import fs from 'fs';
import path from 'path';

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
            phaseShift: phaseShiftStr,
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
        const phaseShift = parseFloat(phaseShiftStr || '0');
        const startNode = param1;
        const endNode = param2;

        // Run up44 once to generate the cost file
        try {
            const projectRoot = process.cwd();
            const up44Path = path.join(projectRoot, 'up44');
            
            if (!fs.existsSync(up44Path)) {
                return c.json({ error: 'up44バイナリが見つかりません' }, 500);
            }
            
            const up44Args = [
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
            
            execSync(`./up44 ${up44Args}`, {
                encoding: 'utf8',
                cwd: projectRoot,
                maxBuffer: 10 * 1024 * 1024,
            });
        } catch (err: any) {
            console.error(`[up44実行エラー] ${err.message}`);
            return c.json({ error: 'up44の実行に失敗しました' }, 500);
        }

        const startNodeInt = parseInt(startNode || '0', 10);
        const endNodeInt = parseInt(endNode || '0', 10);

        if (isNaN(startNodeInt) || isNaN(endNodeInt)) {
            return c.json({ error: 'Invalid start or end node' }, 400);
        }

        // Use C binary for complete calculation
        const startTime = Date.now();
        console.log(`[Cバイナリ計算] 期待値計算からイェンのアルゴリズムまでCバイナリで実行中...`);
        const yenStartTime = Date.now();

        try {
            // yens_algorithmバイナリを実行
            const projectRoot = process.cwd();
            const yensAlgorithmPath = path.join(projectRoot, 'yens_algorithm');
            
            if (!fs.existsSync(yensAlgorithmPath)) {
                return c.json({ error: 'yens_algorithmバイナリが見つかりません' }, 500);
            }
            
            const yensArgs = [
                startNodeInt.toString(),
                endNodeInt.toString(),
                walkingSpeed.toString(),
                phaseShift.toString(),
            ].join(' ');
            
            const cProgramOutput = execSync(`./yens_algorithm ${yensArgs}`, {
                encoding: 'utf8',
                cwd: projectRoot,
                maxBuffer: 10 * 1024 * 1024,
            });

            const yenTime = Date.now() - yenStartTime;
            console.log(`[Cバイナリ計算完了] ${(yenTime / 1000).toFixed(2)}秒`);

            // JSONをパース
            const top5Routes = JSON.parse(cProgramOutput);

            if (!Array.isArray(top5Routes) || top5Routes.length === 0) {
                return c.json([]);
            }

            const totalTime = Date.now() - startTime;
            console.log(`[最終結果] Cバイナリ計算: ${top5Routes.length}件の経路を発見`);
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
            console.log(`[処理時間内訳] Cバイナリ計算: ${(yenTime / 1000).toFixed(2)}秒`);

            return c.json(top5Routes);
        } catch (cErr: any) {
            console.error(`[Cバイナリ実行エラー] ${cErr.message}`);
            const stderr = cErr.stderr?.toString() || '';
            const stdout = cErr.stdout?.toString() || '';
            console.error(`[Cバイナリ実行エラー詳細] stderr: ${stderr}, stdout: ${stdout}`);
            return c.json({ error: 'Cバイナリプログラムの実行に失敗しました' }, 500);
        }
    } catch (err: any) {
        console.error(`実行エラー詳細: ${err.message}`);
        return c.json({ error: err.message, stderr: err.stderr, stdout: err.stdout }, 500);
    }
});

export default calc;
