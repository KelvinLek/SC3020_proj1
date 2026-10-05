// tree_build.h - bulk loading, statistics and validation for the B+ tree (Task 2).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "bplustree.h"

namespace bptree {

// block 0 is the header, so 0 can mean "no next leaf"
constexpr BlockId NO_NEXT_LEAF = 0;

constexpr std::size_t minLeafKeys(std::size_t n) { return (n + 1) / 2; }
constexpr std::size_t minInternalChildren(std::size_t n) { return n / 2 + 1; }

struct Entry {
    float key;
    RecordId rid;
};

// sort by key, then by RecordId so duplicates always come out in the same order
bool entryLess(const Entry& a, const Entry& b);

void bulkLoad(BPlusTree& tree, std::vector<Entry> entries);

struct TreeStats {
    std::size_t n = 0;
    std::size_t numNodes = 0;
    std::size_t numLeaves = 0;
    std::size_t numInternal = 0;
    std::size_t levels = 0;
    std::size_t numEntries = 0;
    std::vector<float> rootKeys;
};

TreeStats treeStats(BPlusTree& tree);

// returns "" if the tree is valid, otherwise what is wrong
std::string validate(BPlusTree& tree, std::vector<Entry> expected);

std::string dumpTree(BPlusTree& tree, std::size_t maxNodesPerLevel = 20);

}  // namespace bptree
