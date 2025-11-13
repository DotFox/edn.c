#!/usr/bin/env pwsh
# EDN.C - PowerShell Build Script
# Cross-platform build script (works on Windows, macOS, Linux)

param(
    [string]$BuildType = "Release",
    [switch]$Clean,
    [switch]$Test,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

Write-Host "EDN.C Build Script" -ForegroundColor Cyan
Write-Host "==================" -ForegroundColor Cyan
Write-Host ""

# Check CMake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: CMake not found" -ForegroundColor Red
    Write-Host "Please install CMake from https://cmake.org/download/" -ForegroundColor Yellow
    exit 1
}

$ProjectRoot = $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"

# Clean if requested
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

# Create build directory
if (-not (Test-Path $BuildDir)) {
    Write-Host "Creating build directory..." -ForegroundColor Green
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Push-Location $BuildDir

try {
    # Configure
    Write-Host ""
    Write-Host "Configuring project..." -ForegroundColor Green
    $ConfigArgs = @("..", "-DCMAKE_BUILD_TYPE=$BuildType")
    
    if ($Verbose) {
        $ConfigArgs += "-DCMAKE_VERBOSE_MAKEFILE=ON"
    }
    
    & cmake @ConfigArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }

    # Build
    Write-Host ""
    Write-Host "Building library and tests..." -ForegroundColor Green
    $BuildArgs = @("--build", ".", "--config", $BuildType)
    
    if ($Verbose) {
        $BuildArgs += "--verbose"
    }
    
    & cmake @BuildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }

    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Build successful!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""

    # Run tests if requested
    if ($Test) {
        Write-Host "Running tests..." -ForegroundColor Cyan
        Write-Host ""
        & ctest -C $BuildType --output-on-failure
        if ($LASTEXITCODE -ne 0) {
            throw "Tests failed"
        }
        Write-Host ""
        Write-Host "All tests passed!" -ForegroundColor Green
    }
    else {
        Write-Host "Library: build/$BuildType/edn.lib (or build/libedn.a)" -ForegroundColor Cyan
        Write-Host "Tests:   build/$BuildType/*.exe (or build/test_*.exe)" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "To run tests:" -ForegroundColor Yellow
        Write-Host "  cd build"
        Write-Host "  ctest -C $BuildType"
        Write-Host ""
        Write-Host "Or with PowerShell:" -ForegroundColor Yellow
        Write-Host "  .\build.ps1 -Test"
        Write-Host ""
    }
}
catch {
    Write-Host ""
    Write-Host "ERROR: $_" -ForegroundColor Red
    Write-Host ""
    Write-Host "Troubleshooting:" -ForegroundColor Yellow
    Write-Host "  - Make sure you have a C compiler installed (Visual Studio, GCC, or Clang)"
    Write-Host "  - If using MSVC, run from 'Developer PowerShell for VS'"
    Write-Host "  - If using MinGW, ensure gcc is in your PATH"
    Write-Host ""
    exit 1
}
finally {
    Pop-Location
}
