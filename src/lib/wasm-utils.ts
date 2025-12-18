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
    const fullPath = path.join(projectRoot, binaryPath);

    if (!fs.existsSync(fullPath)) {
        throw new Error(`バイナリファイルが見つかりません: ${fullPath}`);
    }

    try {
        const command = `./${binaryPath} ${args.join(' ')}`;
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
 * yens_algorithmバイナリを実行
 */
export async function runYen(
    startNode: number,
    endNode: number,
    walkingSpeed: number
): Promise<string> {
    const args = [startNode.toString(), endNode.toString(), walkingSpeed.toString()];
    return await runCBinary('yens_algorithm', args);
}

/**
 * signalバイナリを実行
 */
export async function runSignal(referenceEdge: string, walkingSpeed: number): Promise<string> {
    const projectRoot = process.cwd();
    const binaryFullPath = path.join(projectRoot, 'signal');

    if (!fs.existsSync(binaryFullPath)) {
        throw new Error(`signalバイナリが見つかりません`);
    }

    const args = [referenceEdge, walkingSpeed.toString()];
    return await runCBinary('signal', args);
}
