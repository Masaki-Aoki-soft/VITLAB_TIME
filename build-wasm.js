/**
 * EmscriptenでCプログラムをWASMにコンパイルするスクリプト（Node.js版）
 */

const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');

// Emscriptenがインストールされているか確認
function checkEmscripten() {
    try {
        execSync('emcc --version', { stdio: 'ignore' });
        return true;
    } catch (error) {
        return false;
    }
}

// WASMモジュールをビルド
function buildWASM(sourceFile, outputName, initialMemory = 16777216) {
    console.log(`Building ${outputName}.wasm...`);
    
    const outputPath = path.join(process.cwd(), `${outputName}.wasm`);
    const jsPath = path.join(process.cwd(), `${outputName}.js`);
    
    // WindowsとUnixで引用符の扱いが異なるため、配列形式でコマンドを構築
    // スタンドアロンWASMファイルのみを生成
    const command = [
        'emcc',
        sourceFile,
        `-o`, `${outputName}.wasm`,
        '-s', 'STANDALONE_WASM=1',
        '-s', 'EXPORTED_FUNCTIONS=_main,_malloc,_free',
        '-s', 'ALLOW_MEMORY_GROWTH=1',
        '-s', `INITIAL_MEMORY=${initialMemory}`,
        '-O2'
    ];
    
    try {
        execSync(command.join(' '), { stdio: 'inherit', cwd: process.cwd(), shell: true });
        console.log(`✓ ${outputName}.wasm created successfully`);
        return true;
    } catch (error) {
        console.error(`✗ Failed to build ${outputName}.wasm:`, error.message);
        return false;
    }
}

// メイン処理
function main() {
    console.log('Building WASM modules...\n');
    
    if (!checkEmscripten()) {
        console.error('Error: Emscripten (emcc) is not installed or not in PATH');
        console.error('Please install Emscripten: https://emscripten.org/docs/getting_started/downloads.html');
        process.exit(1);
    }
    
    const projectRoot = process.cwd();
    const sourceFiles = [
        { source: 'user_preference_ver4.4.c', output: 'user_preference_ver4.4', memory: 16777216 },
        { source: 'yens_algorithm.c', output: 'yens_algorithm', memory: 33554432 },
        { source: 'calculate_wait_time.c', output: 'calculate_wait_time', memory: 16777216 }
    ];
    
    let success = true;
    for (const file of sourceFiles) {
        const sourcePath = path.join(projectRoot, file.source);
        if (!fs.existsSync(sourcePath)) {
            console.error(`Error: Source file not found: ${file.source}`);
            success = false;
            continue;
        }
        
        if (!buildWASM(file.source, file.output, file.memory)) {
            success = false;
        }
    }
    
    if (success) {
        console.log('\n✓ Standalone WASM build complete!');
        console.log('Generated files:');
        sourceFiles.forEach(file => {
            console.log(`  - ${file.output}.wasm`);
        });
    } else {
        console.error('\n✗ Some WASM modules failed to build');
        process.exit(1);
    }
}

main();

