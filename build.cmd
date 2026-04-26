@echo off
echo Building BetterBDM...

cd /d "%~dp0"

if not exist "build" mkdir build
cd build

echo [*] Running CMake...
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo [!] CMake configuration failed
    pause
    exit /b 1
)

echo [*] Building...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [!] Build failed
    pause
    exit /b 1
)

echo.
echo ========================================
echo  Build completed!
echo ========================================
echo.
echo Outputs in build\bin\:
echo   - injector.exe
echo   - core.dll
echo   - BDM-Plugin-MediaControl.dll
echo.
pause
