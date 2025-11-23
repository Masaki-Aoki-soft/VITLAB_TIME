@echo off
REM emsdk環境をアクティベートするスクリプト

REM 一般的なemsdkの場所を確認
if exist "C:\emsdk\emsdk.bat" (
    call C:\emsdk\emsdk.bat activate latest
    call C:\emsdk\emsdk_env.bat
    echo Emscripten environment activated from C:\emsdk\
    goto :end
)

if exist "%USERPROFILE%\emsdk\emsdk.bat" (
    call "%USERPROFILE%\emsdk\emsdk.bat" activate latest
    call "%USERPROFILE%\emsdk\emsdk_env.bat"
    echo Emscripten environment activated from %USERPROFILE%\emsdk\
    goto :end
)

echo Error: emsdk not found in common locations.
echo Please run: emsdk activate latest
echo Then run: emsdk_env.bat
:end

