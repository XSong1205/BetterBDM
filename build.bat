@echo off
setlocal enabledelayedexpansion

echo ========================================
echo  BetterBDM Build Script
echo ========================================
echo.

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build
set CMAKE_PATH=C:\Program Files\CMake\bin\cmake.exe

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo [*] Checking for CMake...
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] CMake not found in PATH
    echo [*] Please install CMake 3.16+ and add it to PATH
    echo [*] Download: https://cmake.org/download/
    exit /b 1
)

echo [*] Configuring project...
cd /d "%BUILD_DIR%"
cmake "%SCRIPT_DIR%" -G "Visual Studio 17 2022" -A x64
if %errorlevel% neq 0 (
    echo [!] CMake configuration failed
    exit /b 1
)

echo.
echo [*] Building project...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [!] Build failed
    exit /b 1
)

echo.
echo ========================================
echo  Build completed successfully!
echo ========================================
echo.
echo Outputs:
echo   Injector: %BUILD_DIR%\bin\injector.exe
echo   Core DLL: %BUILD_DIR%\bin\core.dll
echo   Plugin:   %BUILD_DIR%\bin\BDM-Plugin-MediaControl.dll
echo.
echo To inject:
echo   injector.exe --attach -t "波点音乐"
echo.

endlocal
