import { NextRequest, NextResponse } from 'next/server';
import { execSync } from 'child_process';
import fs from 'fs';
import path from 'path';

export async function POST(request: NextRequest) {
  console.log('\n--- [/calculate-wait-time]リクエスト受信 ---');
  try {
    const body = await request.json();
    const { referenceEdge, walkingSpeed } = body;

    if (!referenceEdge || !walkingSpeed) {
      throw new Error('referenceEdgeとwalkingSpeedは必須です。');
    }

    console.log(
      `[OK] パラメータ取得: referenceEdge=${referenceEdge}, walkingSpeed=${walkingSpeed}`
    );

    // Robustness checks
    const projectRoot = process.cwd();
    if (!fs.existsSync(path.join(projectRoot, 'signal_inf.csv')))
      throw new Error('サーバーに signal_inf.csv が見つかりません。');
    if (!fs.existsSync(path.join(projectRoot, 'result2.txt')))
      throw new Error('result2.txt が見つかりません。先に経路を計算してください。');
    console.log('[OK] 必須ファイルの存在を確認');

    // Cプログラムを実行して信号待ち時間を計算
    console.log('[C言語計算] 信号待ち時間をC言語で計算中...');
    try {
      const cProgramOutput = execSync(`./signal ${referenceEdge} ${walkingSpeed}`, {
        encoding: 'utf8',
        cwd: projectRoot,
        maxBuffer: 10 * 1024 * 1024,
      });

      console.log('[OK] C言語計算完了');
      const responsePayload = JSON.parse(cProgramOutput);
      console.log('成功レスポンスを送信:', JSON.stringify(responsePayload));
      return NextResponse.json(responsePayload);
    } catch (cErr: any) {
      console.error(`[C言語実行エラー] ${cErr.message}`);
      console.error(`[C言語実行エラー詳細] ${cErr.stderr || cErr.stdout || ''}`);
      throw new Error('C言語プログラムの実行に失敗しました');
    }
  } catch (err: any) {
    console.error('!!! /calculate-wait-time でエラー発生 !!!');
    console.error('エラーメッセージ:', err.message);
    console.error('スタックトレース:', err.stack);
    const errorPayload = { error: err.message };
    console.log('エラーレスポンスを送信:', JSON.stringify(errorPayload));
    return NextResponse.json(errorPayload, { status: 500 });
  }
}

