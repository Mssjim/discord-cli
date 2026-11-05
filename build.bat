@echo off
setlocal enabledelayedexpansion
echo ===================================================
echo             BUILDING DISCORD CLI (C)
echo ===================================================

:: 1. Locate GCC (MinGW-w64) in PATH or common installation directories
:check_gcc
where gcc >nul 2>nul
if not errorlevel 1 goto :gcc_found

for %%D in (
    "C:\mingw64\bin"
    "C:\msys64\ucrt64\bin"
    "C:\msys64\mingw64\bin"
    "C:\w64devkit\bin"
    "C:\TDM-GCC-64\bin"
    "C:\MinGW\bin"
    "%USERPROFILE%\scoop\apps\mingw\current\bin"
) do (
    if exist "%%~D\gcc.exe" (
        set "MINGW_BIN=%%~D"
        goto :prepend_mingw
    )
)

if exist "%LOCALAPPDATA%\Microsoft\WinGet\Packages" (
    for /d %%P in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs*") do (
        if exist "%%~P\mingw64\bin\gcc.exe" (
            set "MINGW_BIN=%%~P\mingw64\bin"
            goto :prepend_mingw
        )
    )
)

if exist "%LOCALAPPDATA%\Microsoft\WinGet\Links\gcc.exe" (
    set "MINGW_BIN=%LOCALAPPDATA%\Microsoft\WinGet\Links"
    goto :prepend_mingw
)

:: GCC not found - attempt automatic installation with winget
echo GCC (MinGW-w64) compiler not found!
echo Attempting to install MinGW-w64 via winget...
where winget >nul 2>nul
if errorlevel 1 (
    echo.
    echo Error: winget is not available on this system.
    echo Please install MinGW-w64 manually from:
    echo   - Web: https://winlibs.com/ or https://github.com/skeeto/w64devkit/releases
    exit /b 1
)

echo Running: winget install --id BrechtSanders.WinLibs.POSIX.UCRT ...
winget install --id BrechtSanders.WinLibs.POSIX.UCRT -e --source winget --accept-source-agreements --accept-package-agreements
if errorlevel 1 (
    echo.
    echo Error: Failed to automatically install MinGW-w64 via winget.
    echo Please install it manually or check your internet connection.
    exit /b 1
)

:: Re-scan after winget installation
if exist "%LOCALAPPDATA%\Microsoft\WinGet\Links\gcc.exe" (
    set "MINGW_BIN=%LOCALAPPDATA%\Microsoft\WinGet\Links"
    goto :prepend_mingw
)

if exist "%LOCALAPPDATA%\Microsoft\WinGet\Packages" (
    for /d %%P in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs*") do (
        if exist "%%~P\mingw64\bin\gcc.exe" (
            set "MINGW_BIN=%%~P\mingw64\bin"
            goto :prepend_mingw
        )
    )
)

goto :check_gcc

:prepend_mingw
set "PATH=%MINGW_BIN%;%PATH%"

:gcc_found
for /f "delims=" %%G in ('where gcc') do (
    set "GCC_PATH=%%G"
    goto :got_gcc_path
)
:got_gcc_path
echo Found GCC: %GCC_PATH%

:: 2. Locate CMake
set "CMAKE_PATH="
where cmake >nul 2>nul
if not errorlevel 1 (
    set "CMAKE_PATH=cmake"
    goto :check_make
)
if exist "C:\Program Files\CMake\bin\cmake.exe" (
    set "CMAKE_PATH=C:\Program Files\CMake\bin\cmake.exe"
    goto :check_make
)
if exist "%LOCALAPPDATA%\Microsoft\WinGet\Links\cmake.exe" (
    set "CMAKE_PATH=%LOCALAPPDATA%\Microsoft\WinGet\Links\cmake.exe"
    goto :check_make
)
if exist "%LOCALAPPDATA%\Microsoft\WinGet\Packages" (
    for /d %%P in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\Kitware.CMake*") do (
        for /d %%C in ("%%~P\cmake-*") do (
            if exist "%%~C\bin\cmake.exe" (
                set "CMAKE_PATH=%%~C\bin\cmake.exe"
                goto :check_make
            )
        )
    )
)

:check_make
if not defined CMAKE_PATH goto :direct_gcc

:: 3. Select Make/Ninja tool for CMake + MinGW
set "CMAKE_GENERATOR="
set "MAKE_FWD="

where mingw32-make >nul 2>nul
if not errorlevel 1 (
    set "CMAKE_GENERATOR=MinGW Makefiles"
    goto :run_cmake
)

where ninja >nul 2>nul
if not errorlevel 1 (
    set "CMAKE_GENERATOR=Ninja"
    goto :run_cmake
)

where make >nul 2>nul
if not errorlevel 1 (
    for /f "delims=" %%M in ('where make') do (
        set "MAKE_PROG=%%M"
        goto :got_make_prog
    )
)
goto :direct_gcc

:got_make_prog
set "CMAKE_GENERATOR=MinGW Makefiles"
set "MAKE_FWD=%MAKE_PROG:\=/%"

:run_cmake
:: Clean previous CMake cache if it used another generator (e.g. Visual Studio)
if exist "build\CMakeCache.txt" (
    findstr /i /c:"CMAKE_GENERATOR:INTERNAL=%CMAKE_GENERATOR%" "build\CMakeCache.txt" >nul 2>nul
    if errorlevel 1 (
        echo Cleaning previous CMake cache...
        del /f /q "build\CMakeCache.txt" >nul 2>nul
        if exist "build\CMakeFiles" rmdir /s /q "build\CMakeFiles"
    )
)

echo Configuring project with CMake (%CMAKE_GENERATOR%)...
if defined MAKE_FWD (
    "%CMAKE_PATH%" -B build -G "%CMAKE_GENERATOR%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_MAKE_PROGRAM="%MAKE_FWD%"
) else (
    "%CMAKE_PATH%" -B build -G "%CMAKE_GENERATOR%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc
)
if errorlevel 1 (
    echo Error: CMake configuration failed.
    exit /b 1
)

echo Building Release version...
"%CMAKE_PATH%" --build build --config Release
if errorlevel 1 (
    echo Error: CMake build failed.
    exit /b 1
)
goto :verify_binary

:direct_gcc
echo Compiling directly with GCC...
if not exist "build\Release" mkdir "build\Release"
set "RES_OBJ="
where windres >nul 2>nul
if not errorlevel 1 (
    windres src/app.rc -Isrc -O coff -o build/app.res
    if not errorlevel 1 set "RES_OBJ=build/app.res"
)
gcc -std=c99 -O2 -DNDEBUG -D_WIN32_WINNT=0x0A00 -DMINGW_HAS_SECURE_API=1 -Wall -Wno-unknown-pragmas ^
    -Isrc -Ilib/cjson ^
    src/main.c src/config.c src/discord/state.c src/discord/api.c ^
    src/discord/gateway.c src/ui/terminal.c src/ui/views.c lib/cjson/cJSON.c %RES_OBJ% ^
    -lwinhttp -static-libgcc -o "build\Release\discord-cli.exe"
if errorlevel 1 (
    echo Error: Direct GCC compilation failed.
    exit /b 1
)

:verify_binary
if exist "build\Release\discord-cli.exe" (
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
