@echo off
REM EDN.C - Windows Build Script
REM Quick build using CMake (works with MSVC, MinGW, or Clang)

echo EDN.C Windows Build Script
echo ==========================
echo.

REM Check if CMake is available
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake not found. Please install CMake from https://cmake.org/download/
    exit /b 1
)

REM Check if build directory exists
if not exist build (
    echo Creating build directory...
    mkdir build
)

cd build

REM Configure
echo.
echo Configuring project...
cmake .. -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: CMake configuration failed.
    echo.
    echo Troubleshooting:
    echo   - Make sure you have a C compiler installed (Visual Studio, MinGW, or Clang^)
    echo   - If using MSVC, run this script from "Developer Command Prompt for VS"
    echo   - If using MinGW, run from MINGW64 terminal
    echo.
    exit /b 1
)

REM Build
echo.
echo Building library and tests...
cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed.
    exit /b 1
)

echo.
echo ========================================
echo Build successful!
echo ========================================
echo.
echo Library: build\Release\edn.lib (or build\libedn.a)
echo Tests:   build\Release\*.exe (or build\test_*.exe)
echo.
echo To run tests:
echo   cd build
echo   ctest -C Release
echo.
echo To run a single test:
echo   .\Release\test_parser.exe
echo   (or .\test_parser.exe on MinGW)
echo.
echo To run benchmarks:
echo   .\Release\bench_integration.exe
echo.

cd ..
