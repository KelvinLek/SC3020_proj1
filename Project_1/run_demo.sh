#!/usr/bin/env bash
# Rebuilds only demo_inputs. Task 3 creates a fresh run folder every time.
set -euo pipefail
cd -- "$(dirname -- "$0")"
bash build.sh
mkdir -p demo_inputs
./task1.exe games.txt demo_inputs/data.db
./task2.exe demo_inputs/data.db --index demo_inputs/index.db
./task3.exe demo_inputs/data.db demo_inputs/index.db
