import fs from 'fs';
import path from 'path';
import { execSync } from 'child_process';

/**
 * WASMファイルを実行するためのユーティリティ
 * wasmtimeが利用できない場合は、元のCバイナリにフォールバックします
 */

/**
 * Cバイナリを実行（フォールバック）
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
        const stderr = error.stderr?.toString() || '';
        const stdout = error.stdout?.toString() || '';
        throw new Error(`Cバイナリ実行エラー: ${errorMessage}\nstderr: ${stderr}\nstdout: ${stdout}`);
    }
}

/**
 * wasmtimeを使用してWASMファイルを実行（フォールバック）
 */
async function runWasmWithWasmtime(wasmPath: string, args: string[], wasmtimePath: string = 'wasmtime'): Promise<string> {
    const projectRoot = process.cwd();
    const fullPath = path.join(projectRoot, wasmPath);
    
    if (!fs.existsSync(fullPath)) {
        throw new Error(`WASMファイルが見つかりません: ${fullPath}`);
    }

    try {
        const command = `${wasmtimePath} ${fullPath} ${args.join(' ')}`;
        const output = execSync(command, {
            encoding: 'utf8',
            cwd: projectRoot,
            maxBuffer: 10 * 1024 * 1024,
            env: { ...process.env, PATH: `/usr/local/bin:${process.env.PATH}` },
        });
        return output;
    } catch (error: any) {
        const errorMessage = error.message || 'Unknown error';
        const stderr = error.stderr?.toString() || '';
        const stdout = error.stdout?.toString() || '';
        throw new Error(`WASM実行エラー: ${errorMessage}\nstderr: ${stderr}\nstdout: ${stdout}`);
    }
}

/**
 * WASMランタイムを検出して実行
 * wasmtimeが利用できない場合は、元のCバイナリにフォールバック
 */
async function runWasm(wasmPath: string, args: string[], fallbackBinary?: string): Promise<string> {
    // まずwasmtimeを試す（/usr/local/bin/wasmtimeも確認）
    let wasmtimePath = 'wasmtime';
    let hasWasmtime = false;
    
    try {
        execSync('wasmtime --version', { stdio: 'ignore' });
        hasWasmtime = true;
    } catch {
        try {
            execSync('/usr/local/bin/wasmtime --version', { stdio: 'ignore' });
            wasmtimePath = '/usr/local/bin/wasmtime';
            hasWasmtime = true;
        } catch {
            // wasmtimeが利用できない場合、Cバイナリにフォールバック
            if (fallbackBinary) {
                console.warn(`wasmtimeが見つかりません。Cバイナリ（${fallbackBinary}）を使用します。`);
                return await runCBinary(fallbackBinary, args);
            } else {
                throw new Error('wasmtimeがインストールされていません。WASMファイルを実行するには、wasmtimeが必要です。');
            }
        }
    }
    
    // wasmtimePathを使用して実行
    if (hasWasmtime) {
        try {
            return await runWasmWithWasmtime(wasmPath, args, wasmtimePath);
        } catch (error: any) {
            // wasmtimeの実行に失敗した場合、Cバイナリにフォールバック
            if (fallbackBinary) {
                console.warn(`wasmtimeの実行に失敗しました。Cバイナリ（${fallbackBinary}）を使用します。`);
                return await runCBinary(fallbackBinary, args);
            } else {
                throw error;
            }
        }
    }
    
    // フォールバック: Cバイナリを実行
    if (fallbackBinary) {
        return await runCBinary(fallbackBinary, args);
    }
    
    throw new Error('wasmtimeがインストールされていません。WASMファイルを実行するには、wasmtimeが必要です。');
}

/**
 * up44.wasmを実行（フォールバック: up44）
 */
export async function runUp44(args: string[]): Promise<void> {
    await runWasm('up44.wasm', args, 'up44');
}

/**
 * yen.wasmを実行（フォールバック: yens_algorithm）
 */
export async function runYen(startNode: number, endNode: number, walkingSpeed: number, phaseShift: number = 0): Promise<string> {
    const args = [startNode.toString(), endNode.toString(), walkingSpeed.toString(), phaseShift.toString()];
    return await runWasm('yen.wasm', args, 'yens_algorithm');
}

/**
 * signal.wasmを実行（フォールバック: signal）
 */
export async function runSignal(referenceEdge: string, walkingSpeed: number): Promise<string> {
    const signalWasmPath = 'signal.wasm';
    const projectRoot = process.cwd();
    const wasmFullPath = path.join(projectRoot, signalWasmPath);
    const binaryFullPath = path.join(projectRoot, 'signal');
    
    // WASMファイルまたはCバイナリのいずれかが存在することを確認
    if (!fs.existsSync(wasmFullPath) && !fs.existsSync(binaryFullPath)) {
        throw new Error(`signal.wasmまたはsignalが見つかりません`);
    }
    
    const args = [referenceEdge, walkingSpeed.toString()];
    return await runWasm(signalWasmPath, args, 'signal');
}

