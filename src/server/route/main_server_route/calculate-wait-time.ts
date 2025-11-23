import { Hono } from 'hono';
import fs from 'fs';
import path from 'path';
import { loadWASMModule, writeFileToWASMFS, readFileFromWASMFS } from '../../../lib/wasm-loader';

const calc_wait_time = new Hono().post('/calculateWaitTime', async (c) => {
    console.log('\n--- [/calculate-wait-time]リクエスト受信 ---');
    try {
        const body = await c.req.json();
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

        // WASMプログラムを実行して信号待ち時間を計算
        console.log('[WASM計算] 信号待ち時間をWASMで計算中...');
        try {
            // calculate_wait_time WASMモジュールを読み込む
            console.log('[WASM] calculate_wait_timeモジュールを読み込み中...');
            const signalModule = await loadWASMModule('calculate_wait_time');
            
            // 必要なファイルをWASMの仮想ファイルシステムにコピー
            const routeDataFile = path.join(projectRoot, 'oomiya_route_inf_4.csv');
            const routeData = fs.readFileSync(routeDataFile, 'utf8');
            writeFileToWASMFS(signalModule, 'oomiya_route_inf_4.csv', routeData);
            
            const signalDataFile = path.join(projectRoot, 'signal_inf.csv');
            const signalData = fs.readFileSync(signalDataFile, 'utf8');
            writeFileToWASMFS(signalModule, 'signal_inf.csv', signalData);
            
            const result2File = path.join(projectRoot, 'result2.txt');
            const result2Data = fs.readFileSync(result2File, 'utf8');
            writeFileToWASMFS(signalModule, 'result2.txt', result2Data);

            // calculate_wait_timeを実行
            let stdout = '';
            const originalPrint = signalModule.print;
            signalModule.print = (text: string) => {
                stdout += text;
            };
            
            try {
                signalModule.callMain(['calculate_wait_time', referenceEdge, String(walkingSpeed)]);
            } finally {
                signalModule.print = originalPrint;
            }

            console.log('[OK] WASM計算完了');
            
            // JSONをパース（標準出力から取得）
            const jsonMatch = stdout.match(/\{[\s\S]*\}/);
            if (!jsonMatch) {
                throw new Error('JSON output not found in WASM output');
            }
            
            const responsePayload = JSON.parse(jsonMatch[0]);
            console.log('成功レスポンスを送信:', JSON.stringify(responsePayload));
            return c.json(responsePayload);
        } catch (cErr: any) {
            console.error(`[WASM実行エラー] ${cErr.message}`);
            console.error(`[WASM実行エラー詳細] ${cErr.stack || ''}`);
            throw new Error('WASMプログラムの実行に失敗しました');
        }
    } catch (err: any) {
        console.error('!!! /calculate-wait-time でエラー発生 !!!');
        console.error('エラーメッセージ:', err.message);
        console.error('スタックトレース:', err.stack);
        const errorPayload = { error: err.message };
        console.log('エラーレスポンスを送信:', JSON.stringify(errorPayload));
        return c.json(errorPayload, 500);
    }
});

export default calc_wait_time;
