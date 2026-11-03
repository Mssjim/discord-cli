@echo off
setlocal
echo ===================================================
echo             BUILDING DISCORD CLI (C)
echo ===================================================

where cmake >nul 2>nul
if errorlevel 1 (
    echo CMake not found. Attempting to install it with winget...
    where winget >nul 2>nul
    if errorlevel 1 (
        echo Error: winget is not available. Install CMake manually from https://cmake.org/download/
        exit /b 1
    )

    winget install --id Kitware.CMake -e --source winget --accept-source-agreements --accept-package-agreements
    if errorlevel 1 (
        echo Error: Failed to install CMake with winget.
        exit /b 1
    )
)

where cmake >nul 2>nul
if not errorlevel 1 (
    set "CMAKE_PATH=cmake"
) else if exist "C:\Program Files\CMake\bin\cmake.exe" (
    set "CMAKE_PATH=C:\Program Files\CMake\bin\cmake.exe"
) else (
    echo Error: CMake is installed but could not be located.
    echo Restart this command prompt and try again.
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
