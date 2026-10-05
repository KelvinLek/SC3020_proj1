#!/usr/bin/env bash
# Run in MSYS2 UCRT64 on Windows: bash build.sh
set -euo pipefail
cd -- "$(dirname -- "$0")"
if ! command -v g++ >/dev/null 2>&1; then
  echo 'g++ was not found. In MSYS2 UCRT64 run:'
  echo 'pacman -S --needed mingw-w64-ucrt-x86_64-gcc'
  exit 1
fi
flags=(-std=c++17 -O2 -Wall -Wextra -IStorage/src)
storage=(Storage/src/disk.cpp Storage/src/record.cpp Storage/src/storage.cpp)
nodes=(BPlusTree/src/bplustree_header_io.cpp BPlusTree/src/bplustree_leaf.cpp BPlusTree/src/bplustree_internal.cpp BPlusTree/src/bplustree_debug.cpp)
tree=("${nodes[@]}" BPlusTree/src/bulk_load.cpp BPlusTree/src/tree_stats.cpp)
echo 'Compiling Task 1...'
g++ "${flags[@]}" -o task1.exe Storage/src/task1.cpp "${storage[@]}"
echo 'Compiling Task 2...'
g++ "${flags[@]}" -o task2.exe BPlusTree/src/task2.cpp "${tree[@]}" "${storage[@]}"
echo 'Compiling Task 3...'
g++ "${flags[@]}" -o task3.exe BPlusTree/src/task3.cpp BPlusTree/src/bplustree_delete.cpp "${tree[@]}" "${storage[@]}"
echo 'Build successful.'
