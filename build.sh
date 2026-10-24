#!/usr/bin/env bash
set -euo pipefail

echo "============================================================"
echo " HeteroSched Build & Test Suite (Linux / GCC / CMake)"
echo "============================================================"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "Using CMake: $(cmake --version | head -n 1)"
echo "Using Compiler: $(c++ --version | head -n 1)"

# Configure
echo -e "\n[1/3] Configuring CMake project..."
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
echo -e "\n[2/3] Building targets..."
cmake --build build --config Release -j "$(nproc)"

# Run Tests
echo -e "\n[3/3] Running Automated Tests..."
./build/heterosched_tests
echo -e "\nAll tests completed with status: SUCCESS!"
