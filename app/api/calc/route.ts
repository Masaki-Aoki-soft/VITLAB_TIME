import { NextRequest, NextResponse } from 'next/server';
import { execSync } from 'child_process';
import path from 'path';
import fs from 'fs';

export async function POST(request: NextRequest) {
  try {
    const body = await request.text();
    const params = new URLSearchParams(body);

    const base_args = [
      params.get('weight0'),
      params.get('weight1'),
      params.get('weight2'),
      params.get('weight3'),
      params.get('weight4'),
      params.get('weight5'),
      params.get('weight6'),
      params.get('weight7'),
      params.get('weight8'),
      params.get('weight9'),
      params.get('weight10'),
      params.get('weight11'),
      params.get('weight12'),
      params.get('param1'),
      params.get('param2'),
    ].join(' ');

    const walkingSpeed = parseFloat(params.get('walkingSpeed') || '80');
    const startNode = params.get('param1');
    const endNode = params.get('param2');

    // Run up44 once to generate the cost file
    const projectRoot = process.cwd();
    execSync(`./up44 ${base_args}`, { encoding: 'utf8', cwd: projectRoot });

    const startNodeInt = parseInt(startNode || '0', 10);
    const endNodeInt = parseInt(endNode || '0', 10);

    if (isNaN(startNodeInt) || isNaN(endNodeInt)) {
      return NextResponse.json(
        { error: 'Invalid start or end node' },
        { status: 400 }
      );
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
      const top5Routes = JSON.parse(cProgramOutput);

      if (!Array.isArray(top5Routes) || top5Routes.length === 0) {
        return NextResponse.json([]);
      }

      const totalTime = Date.now() - startTime;
      console.log(`[最終結果] C言語計算: ${top5Routes.length}件の経路を発見`);
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
        `[総処理時間] ${(totalTime / 1000).toFixed(2)}秒（${(
          totalTime / 60000
        ).toFixed(2)}分）`
      );
      console.log(`[処理時間内訳] C言語計算: ${(yenTime / 1000).toFixed(2)}秒`);

      return NextResponse.json(top5Routes);
    } catch (cErr: any) {
      console.error(`[C言語実行エラー] ${cErr.message}`);
      console.error(`[C言語実行エラー詳細] ${cErr.stderr || cErr.stdout || ''}`);
      return NextResponse.json(
        { error: 'C言語プログラムの実行に失敗しました' },
        { status: 500 }
      );
    }
  } catch (err: any) {
    console.error(`実行エラー詳細: ${err.message}`);
    return NextResponse.json(
      { error: err.message, stderr: err.stderr, stdout: err.stdout },
      { status: 500 }
    );
  }
}

