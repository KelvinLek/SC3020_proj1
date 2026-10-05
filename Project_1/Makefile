# Makefile for SC3020 Project 1
# Run from Project_1/
# Usage:
#   make          - build all tasks
#   make run      - build and run full pipeline
#   make clean    - remove built files

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -IStorage/src

STORAGE = Storage/src/disk.cpp \
          Storage/src/record.cpp \
          Storage/src/storage.cpp

NODES   = BPlusTree/src/bplustree_header_io.cpp \
          BPlusTree/src/bplustree_leaf.cpp \
          BPlusTree/src/bplustree_internal.cpp \
          BPlusTree/src/bplustree_debug.cpp

TREE    = $(NODES) \
          BPlusTree/src/bulk_load.cpp \
          BPlusTree/src/tree_stats.cpp

HDRS    = $(wildcard Storage/src/*.h BPlusTree/src/*.h)

.PHONY: all task1 task2 task3 run clean

all: task1 task2 task3

task1: Storage/src/task1.cpp $(STORAGE) $(HDRS)
	$(CXX) $(CXXFLAGS) -o task1 Storage/src/task1.cpp $(STORAGE)

task2: BPlusTree/src/task2.cpp $(TREE) $(STORAGE) $(HDRS)
	$(CXX) $(CXXFLAGS) -o task2 BPlusTree/src/task2.cpp $(TREE) $(STORAGE)

task3: BPlusTree/src/task3.cpp BPlusTree/src/bplustree_delete.cpp $(TREE) $(STORAGE) $(HDRS)
	$(CXX) $(CXXFLAGS) -o task3 \
	    BPlusTree/src/task3.cpp \
	    BPlusTree/src/bplustree_delete.cpp \
	    $(TREE) $(STORAGE)

run: all
	@mkdir -p out
	./task1 games.txt out/data.db
	./task2 out/data.db --index out/index.db
	./task3 out/data.db out/index.db

clean:
	rm -f task1 task2 task3 task1.exe task2.exe task3.exe
	rm -rf out/ task3_runs/ demo_inputs/
