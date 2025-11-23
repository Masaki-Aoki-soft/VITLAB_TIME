/**
 * WASMモジュールを読み込むためのユーティリティ
 */

import fs from 'fs';
import path from 'path';

export interface WASMModule {
    FS: {
        writeFile: (filepath: string, data: string | Uint8Array) => void;
        readFile: (filepath: string, options?: { encoding: 'utf8' }) => string | Uint8Array;
        mkdirTree: (dirpath: string) => void;
        analyzePath: (filepath: string) => { exists: boolean };
    };
    callMain: (args: string[]) => number;
    print: (text: string) => void;
    printErr: (text: string) => void;
    onRuntimeInitialized: () => void;
    _main: (argc: number, argv: number) => number;
}

let moduleCache: Map<string, WASMModule> = new Map();

/**
 * WASMモジュールを読み込む（Node.js環境用）
 * Emscriptenが生成したJSファイルを動的に読み込む
 */
export async function loadWASMModule(moduleName: string): Promise<WASMModule> {
    if (moduleCache.has(moduleName)) {
        return moduleCache.get(moduleName)!;
    }

    const projectRoot = process.cwd();
    const jsPath = path.join(projectRoot, `${moduleName}.js`);
    
    if (!fs.existsSync(jsPath)) {
        throw new Error(`WASM module file not found: ${jsPath}. Please compile the C program to WASM first using "npm run build:wasm".`);
    }

    // Emscriptenが生成したJSファイルを動的に読み込む
    // モジュールを削除してから再読み込み
    let resolvedPath: string;
    try {
        resolvedPath = require.resolve(jsPath);
        if (require.cache[resolvedPath]) {
            delete require.cache[resolvedPath];
        }
    } catch (error) {
        // モジュールがまだ読み込まれていない場合
        resolvedPath = jsPath;
    }
    
    // Emscriptenモジュールを読み込む
    // Emscriptenが生成するJSファイルは、モジュールファクトリー関数をエクスポートします
    const ModuleFactory = require(resolvedPath);
    
    // モジュールを初期化（Promiseでラップ）
    const module = await new Promise<WASMModule>((resolve, reject) => {
        let stdout = '';
        let stderr = '';
        
        // Emscriptenモジュールの設定
        const moduleConfig: any = {
            print: (text: string) => {
                stdout += text;
            },
            printErr: (text: string) => {
                stderr += text;
                console.error(`[${moduleName}]`, text);
            },
            onRuntimeInitialized: () => {
                const wasmModule: WASMModule = {
                    FS: moduleConfig.FS,
                    callMain: moduleConfig.callMain.bind(moduleConfig),
                    print: moduleConfig.print,
                    printErr: moduleConfig.printErr,
                    onRuntimeInitialized: moduleConfig.onRuntimeInitialized,
                    _main: moduleConfig._main,
                };
                moduleCache.set(moduleName, wasmModule);
                resolve(wasmModule);
            }
        };
        
        // モジュールを初期化
        try {
            const ModuleInstance = ModuleFactory(moduleConfig);
            
            // モジュールが既に初期化済みの場合（同期初期化）
            if (ModuleInstance && ModuleInstance.FS) {
                const wasmModule: WASMModule = {
                    FS: ModuleInstance.FS,
                    callMain: ModuleInstance.callMain.bind(ModuleInstance),
                    print: ModuleInstance.print || ((text: string) => { stdout += text; }),
                    printErr: ModuleInstance.printErr || console.error,
                    onRuntimeInitialized: () => {},
                    _main: ModuleInstance._main,
                };
                moduleCache.set(moduleName, wasmModule);
                resolve(wasmModule);
            } else if (ModuleInstance && ModuleInstance.ready) {
                // 非同期初期化
                ModuleInstance.ready.then(() => {
                    const wasmModule: WASMModule = {
                        FS: ModuleInstance.FS,
                        callMain: ModuleInstance.callMain.bind(ModuleInstance),
                        print: ModuleInstance.print || ((text: string) => { stdout += text; }),
                        printErr: ModuleInstance.printErr || console.error,
                        onRuntimeInitialized: () => {},
                        _main: ModuleInstance._main,
                    };
                    moduleCache.set(moduleName, wasmModule);
                    resolve(wasmModule);
                }).catch(reject);
            }
            // onRuntimeInitializedが呼ばれるまで待つ
        } catch (error) {
            reject(new Error(`Failed to initialize WASM module ${moduleName}: ${error}`));
        }
    });

    return module;
}

/**
 * ファイルをWASMの仮想ファイルシステムに書き込む
 */
export function writeFileToWASMFS(module: WASMModule, filepath: string, data: string | Uint8Array): void {
    const FS = module.FS;
    if (!FS) {
        throw new Error('FS is not available in WASM module');
    }
    
    // ディレクトリが存在しない場合は作成
    const dirPath = filepath.substring(0, filepath.lastIndexOf('/'));
    if (dirPath && !FS.analyzePath(dirPath).exists) {
        FS.mkdirTree(dirPath);
    }
    
    // ファイルを書き込む
    FS.writeFile(filepath, data);
}

/**
 * WASMの仮想ファイルシステムからファイルを読み込む
 */
export function readFileFromWASMFS(module: WASMModule, filepath: string): string {
    const FS = module.FS;
    if (!FS) {
        throw new Error('FS is not available in WASM module');
    }
    
    return FS.readFile(filepath, { encoding: 'utf8' }) as string;
}

/**
 * 標準出力をキャプチャして文字列として返す
 */
export function captureStdout(module: WASMModule, callback: () => void): string {
    let output = '';
    const originalPrint = module.print;
    
    module.print = (text: string) => {
        output += text;
    };
    
    try {
        callback();
    } finally {
        module.print = originalPrint;
    }
    
    return output;
}
