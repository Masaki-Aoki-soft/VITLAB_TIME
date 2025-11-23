@echo off
REM emsdk環境をアクティベートしてからWASMをビルドするスクリプト

REM C:\emsdkから環境をアクティベート
if exist "C:\emsdk\emsdk_env.bat" (
    echo Activating Emscripten environment from C:\emsdk...
    call C:\emsdk\emsdk_env.bat
    echo.
    echo Building WASM modules...
    call build-wasm.bat
) else (
    echo Error: emsdk_env.bat not found at C:\emsdk\emsdk_env.bat
    echo Please check if emsdk is installed at C:\emsdk
    exit /b 1
)

