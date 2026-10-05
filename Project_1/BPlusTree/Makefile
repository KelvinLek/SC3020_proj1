CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -I../Storage/src
STORAGE  = ../Storage/src/disk.cpp ../Storage/src/record.cpp ../Storage/src/storage.cpp
NODES    = src/bplustree_header_io.cpp src/bplustree_leaf.cpp src/bplustree_internal.cpp src/bplustree_debug.cpp
TREE     = $(NODES) src/bulk_load.cpp src/tree_stats.cpp

# Task 2: build the tree on the full data set and print the statistics.
task2: src/task2.cpp $(TREE) $(STORAGE) $(wildcard src/*.h ../Storage/src/*.h)
	$(CXX) $(CXXFLAGS) -o task2 src/task2.cpp $(TREE) $(STORAGE)

# Member 2's demo: creates an empty index.db and prints the node layout.
bplustree: src/main.cpp $(NODES) $(STORAGE) $(wildcard src/*.h ../Storage/src/*.h)
	$(CXX) $(CXXFLAGS) -o bplustree src/main.cpp $(NODES) $(STORAGE)

run: task2
	./task2 ../Storage/data.db

clean:
	rm -f task2 task2.exe bplustree bplustree.exe index.db
