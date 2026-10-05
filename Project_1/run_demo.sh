#!/usr/bin/env bash
# run full pipeline: task1 -> task2 -> task3
set -euo pipefail
cd -- "$(dirname -- "$0")"

bash build.sh

mkdir -p out

echo "--- Task 1: load data ---"
./task1 games.txt out/data.db

echo "--- Task 2: build index ---"
./task2 out/data.db --index out/index.db

echo "--- Task 3: retrieval + linear scan + deletion ---"
./task3 out/data.db out/index.db
