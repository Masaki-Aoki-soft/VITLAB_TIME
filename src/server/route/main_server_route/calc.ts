/* コストと信号まの待ち時間を計算するAPI */

import { Hono } from 'hono';
import { runUp44, runYen } from '@/lib/exec-utils';
import fs from 'fs';
import path from 'path';

// Edge data interface
interface EdgeInfo {
    distance: number;
    gradient: number;
}

// Helper to load edge data
function loadEdgeData(): Map<string, EdgeInfo> {
    const map = new Map<string, EdgeInfo>();
    try {
        const csvPath = path.join(process.cwd(), 'oomiya_route_inf_4.csv');
        const content = fs.readFileSync(csvPath, 'utf8');
        const lines = content.split('\n');
        
        // Skip header
        for (let i = 1; i < lines.length; i++) {
            const line = lines[i].trim();
            if (!line) continue;
            const parts = line.split(',');
            if (parts.length < 5) continue;
            
            const n1 = parseInt(parts[0]);
            const n2 = parseInt(parts[1]);
            const dist = parseFloat(parts[2]);
            const grad = parseFloat(parts[4]);
            
            if (!isNaN(n1) && !isNaN(n2) && !isNaN(dist) && !isNaN(grad)) {
                // Normalize key: smaller-larger
                const key = n1 < n2 ? `${n1}-${n2}` : `${n2}-${n1}`;
                map.set(key, { distance: dist, gradient: grad });
            }
        }
    } catch (e) {
        console.error('Error loading edge data:', e);
    }
    return map;
}

const calc = new Hono().post('/calc', async (c) => {
    try {
        const body = await c.req.json();
        const {
            weight0, weight1, weight2, weight3, weight4, weight5, weight6,
            weight7, weight8, weight9, weight10, weight11, weight12,
            param1, param2, walkingSpeed: walkingSpeedStr,
            kGradient: kGradientStr,
        } = body;

        const walkingSpeed = parseFloat(walkingSpeedStr || '80');
        const kGradient = kGradientStr !== undefined ? parseFloat(kGradientStr) : 0.5;
        const startNode = param1;
        const endNode = param2;

        // Run up44 binary once to generate the cost file
        try {
            await runUp44([
                weight0, weight1, weight2, weight3, weight4, weight5, weight6,
                weight7, weight8, weight9, weight10, weight11, weight12,
                param1, param2,
            ]);
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
        console.log(`[Cバイナリ計算] 期待値計算からイェンのアルゴリズムまでCバイナリで実行中... (kGradient=${kGradient})`);
        const yenStartTime = Date.now();

        try {
            // yens_algorithmバイナリを実行
            const cProgramOutput = await runYen(startNodeInt, endNodeInt, walkingSpeed, kGradient);

            const yenTime = Date.now() - yenStartTime;
            console.log(`[Cバイナリ計算完了] ${(yenTime / 1000).toFixed(2)}秒`);

            // JSONをパース
            const top5Routes = JSON.parse(cProgramOutput);

            if (!Array.isArray(top5Routes) || top5Routes.length === 0) {
                return c.json([]);
            }

            // Load edge data for gradient calculation
            const edgeMap = loadEdgeData();
            
            // Calculate gradient diff for Blue Route (routeType 0)
            // (Actually we can do it for all routes if we want, but user asked specifically)
            top5Routes.forEach((route: any) => {
                // Determine edges from userPref
                // format: "22-25.geojson\n25-26.geojson"
                if (!route.userPref) return;

                let flatTimeTotal = 0;
                let gradientTimeTotal = 0;
                let calculatedEdges = 0;
                
                const segments = route.userPref.split('\n');
                for (const seg of segments) {
                    const trimmed = seg.trim();
                    if (!trimmed) continue;
                    
                    // Extract node IDs from filename "22-25.geojson"
                    const match = trimmed.match(/^(\d+)-(\d+)\.geojson$/);
                    if (match) {
                        const n1 = parseInt(match[1]);
                        const n2 = parseInt(match[2]);
                        const key = n1 < n2 ? `${n1}-${n2}` : `${n2}-${n1}`;
                        
                        const edge = edgeMap.get(key);
                        if (edge) {
                            // Calculate times
                            // Flat time: distance / walkingSpeed
                            const flatTime = edge.distance / walkingSpeed * 60.0; // seconds
                            
                            // Gradient time
                            // const kGradient = 0.5; // received from body
                            const adjustedSpeed = walkingSpeed * (1.0 - kGradient * edge.gradient);
                            let gradTime = 0;
                            if (adjustedSpeed > 0) {
                                gradTime = edge.distance / adjustedSpeed * 60.0; // seconds
                            } else {
                                gradTime = flatTime * 10.0; // Fallback penalty
                            }
                            
                            flatTimeTotal += flatTime;
                            gradientTimeTotal += gradTime;
                            calculatedEdges++;
                        }
                    }
                }

                if (calculatedEdges > 0) {
                    // Diff: Time with gradient - Time without gradient
                    // Standard totalTime includes gradient.
                    // So this diff represents "how much extra time" (or less time) is due to gradient.
                    route.totalGradientDiff = gradientTimeTotal - flatTimeTotal;
                }
            });

            const totalTime = Date.now() - startTime;
            console.log(`[最終結果] Cバイナリ計算: ${top5Routes.length}件の経路を発見`);
            console.log(`[最終結果] 上位${top5Routes.length}件の経路を選択`);
            top5Routes.forEach((route: any, index: number) => {
                console.log(
                    `[経路${index + 1}] 総推定時間: ${route.totalTime.toFixed(2)}分, 距離: ${route.totalDistance.toFixed(2)}m, 待ち時間: ${route.totalWaitTime.toFixed(2)}分`
                );
            });
            console.log(
                `[総処理時間] ${(totalTime / 1000).toFixed(2)}秒（${(totalTime / 60000).toFixed(2)}分）`
            );
            console.log(`[処理時間内訳] Cバイナリ計算: ${(yenTime / 1000).toFixed(2)}秒`);

            return c.json(top5Routes);
        } catch (cErr: any) {
            console.error(`[Cバイナリ実行エラー] ${cErr.message}`);
            return c.json({ error: 'Cバイナリプログラムの実行に失敗しました' }, 500);
        }
    } catch (err: any) {
        console.error(`実行エラー詳細: ${err.message}`);
        return c.json({ error: err.message }, 500);
    }
});

export default calc;
