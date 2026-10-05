CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
STORAGE  = src/disk.cpp src/record.cpp src/storage.cpp

task1: src/task1.cpp $(STORAGE) $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) -o task1 src/task1.cpp $(STORAGE)

run: task1
	./task1 ../games.txt data.db --show-block 1

clean:
	rm -f task1 task1.exe data.db
