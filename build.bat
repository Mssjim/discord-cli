@echo off
setlocal
echo ===================================================
echo             BUILDING DISCORD CLI (C)
echo ===================================================

where cmake >nul 2>nul
if %errorlevel% neq 0 (
    set "CMAKE_PATH=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
) else (
    set "CMAKE_PATH=cmake"
)

if not exist "%CMAKE_PATH%" (
    echo Error: CMake not found!
    exit /b 1
)

echo Configuring project with CMake...
"%CMAKE_PATH%" -B build -G "Visual Studio 18 2026"

echo Building Release version...
"%CMAKE_PATH%" --build build --config Release

if exist build\Release\discord-cli.exe (
    echo.
    echo ===================================================
    echo   Build succeeded!
    echo   Binary: build\Release\discord-cli.exe
    echo ===================================================
) else (
    echo.
    echo Error: Failed to generate executable binary.
    exit /b 1
)
