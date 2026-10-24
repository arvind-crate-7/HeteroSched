$ErrorActionPreference = "Stop"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " HeteroSched Build & Test Suite (Windows MinGW / CMake)" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

# Set environment PATH
$cmakeBin = "D:\Hectrogeneous workload Sheduler\cmake_tool\cmake-3.30.5-windows-x86_64\bin"
$mingwBin = "C:\msys64\mingw64\bin"

if (Test-Path $cmakeBin) {
    $env:PATH = "$cmakeBin;$mingwBin;$env:PATH"
} elseif (Test-Path $mingwBin) {
    $env:PATH = "$mingwBin;$env:PATH"
}

Write-Host "Using CMake: $(cmake --version | Select-Object -First 1)" -ForegroundColor Green
Write-Host "Using Compiler: $(g++ --version | Select-Object -First 1)" -ForegroundColor Green

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

# Configure
Write-Host "`n[1/3] Configuring CMake project with MinGW Makefiles..." -ForegroundColor Yellow
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# Build
Write-Host "`n[2/3] Building heterosched_core, heterosched_cli, and heterosched_tests..." -ForegroundColor Yellow
cmake --build build --config Release -j 4

# Run Tests
Write-Host "`n[3/3] Running Automated Tests..." -ForegroundColor Yellow
$testExe = "build/heterosched_tests.exe"
if (Test-Path $testExe) {
    & $testExe
    Write-Host "`nAll tests completed with status: SUCCESS!" -ForegroundColor Green
} else {
    Write-Error "Test binary not found at $testExe"
}
