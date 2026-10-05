#!/usr/bin/env bash
# build all three tasks
set -euo pipefail
cd -- "$(dirname -- "$0")"

CXX="${CXX:-g++}"
FLAGS=(-std=c++17 -O2 -Wall -Wextra -IStorage/src)
STORAGE=(Storage/src/disk.cpp Storage/src/record.cpp Storage/src/storage.cpp)
NODES=(BPlusTree/src/bplustree_header_io.cpp
       BPlusTree/src/bplustree_leaf.cpp
       BPlusTree/src/bplustree_internal.cpp
       BPlusTree/src/bplustree_debug.cpp)
TREE=("${NODES[@]}" BPlusTree/src/bulk_load.cpp BPlusTree/src/tree_stats.cpp)

echo 'Building task1...'
"$CXX" "${FLAGS[@]}" -o task1 Storage/src/task1.cpp "${STORAGE[@]}"

echo 'Building task2...'
"$CXX" "${FLAGS[@]}" -o task2 BPlusTree/src/task2.cpp "${TREE[@]}" "${STORAGE[@]}"

echo 'Building task3...'
"$CXX" "${FLAGS[@]}" -o task3 \
    BPlusTree/src/task3.cpp \
    BPlusTree/src/bplustree_delete.cpp \
    "${TREE[@]}" "${STORAGE[@]}"

echo 'Done.'
