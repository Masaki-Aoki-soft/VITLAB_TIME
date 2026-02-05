import fs from 'fs';
import path from 'path';
import { execSync } from 'child_process';

/**
 * Cバイナリファイルを実行するためのユーティリティ
 */

/**
 * Cバイナリを実行
 */
async function runCBinary(binaryPath: string, args: string[]): Promise<string> {
    const projectRoot = process.cwd();
    // Docker環境での実行を前提とするため、OS判定やWSLコマンドは不要
    // 単純に相対パスで実行する
    const command = `./${binaryPath} ${args.join(' ')}`;

    try {
        const output = execSync(command, {
            encoding: 'utf8',
            cwd: projectRoot,
            maxBuffer: 10 * 1024 * 1024,
        });
        return output;
    } catch (error: any) {
        const errorMessage = error.message || 'Unknown error';
        throw new Error(`Cバイナリ実行エラー: ${errorMessage}`);
    }
}

/**
 * up44バイナリを実行
 */
export async function runUp44(args: string[]): Promise<void> {
    await runCBinary('up44', args);
}

/**
 * yenバイナリを実行
 */
export async function runYen(
    startNode: number,
    endNode: number,
    walkingSpeed: number,
    kGradient?: number
): Promise<string> {
    // バイナリは3つの引数のみ受け付ける: start_node, end_node, walking_speed
    const args = [startNode.toString(), endNode.toString(), walkingSpeed.toString()];
    return await runCBinary('yen', args);
}

/**
 * signalバイナリを実行
 */
export async function runSignal(referenceEdge: string, walkingSpeed: number): Promise<string> {
    const args = [referenceEdge, walkingSpeed.toString()];
    return await runCBinary('signal', args);
}
