#!/usr/bin/env bash
# Build Debug and run tests (includes EXIF time format tests)
set -e
cd "$(dirname "$0")/build"
echo "== Building Debug..."
cmake --build . --config Debug
echo ""
echo "== Running tests (--test)..."
./Debug/FileTimeFixer.exe --test
echo ""
echo "== Done. Exit code: $?"
