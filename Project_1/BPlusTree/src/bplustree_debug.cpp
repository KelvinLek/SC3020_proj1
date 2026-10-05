#include "bplustree.h"

#include <iostream>

namespace bptree {

// ============================================================
// Print design information
// ============================================================

void BPlusTree::printDesign() const {

    std::cout
        << "=== B+ Tree Design ===\n\n";

    std::cout
        << "Block size                 : "
        << NODE_SIZE
        << " bytes\n";

    std::cout
        << "Key                       : FG_PCT_home\n";

    std::cout
        << "Key size                  : "
        << KEY_SIZE
        << " bytes\n";

    std::cout
        << "RecordId size             : "
        << storage::RECORD_ID_SIZE
        << " bytes\n";

    std::cout
        << "Leaf entry size           : "
        << LEAF_ENTRY_SIZE
        << " bytes\n";

    std::cout
        << "Leaf header size          : "
        << LEAF_HEADER_SIZE
        << " bytes\n";

    std::cout
        << "B+ tree parameter n       : "
        << BPLUS_N
        << "\n";

    std::cout
        << "Max keys per leaf         : "
        << BPLUS_N
        << "\n";

    std::cout
        << "Max pointers per internal: "
        << BPLUS_N + 1
        << "\n";

    std::cout
        << "Max keys per internal    : "
        << BPLUS_N
        << "\n";

    std::cout
        << "Internal node size       : "
        << INTERNAL_NODE_SIZE
        << " bytes\n";

    std::cout
        << "Unused leaf bytes        : "
        << NODE_SIZE -
           LEAF_HEADER_SIZE -
           BPLUS_N * LEAF_ENTRY_SIZE
        << "\n";
}


// ============================================================
// Print current header
// ============================================================

void BPlusTree::printHeader() const {

    std::cout
        << "\n=== B+ Tree Header ===\n";

    std::cout
        << "Root node      : "
        << header_.rootNode
        << "\n";

    std::cout
        << "Number of nodes: "
        << header_.numNodes
        << "\n";

    std::cout
        << "Number of levels: "
        << header_.numLevels
        << "\n";

    std::cout
        << "Number of keys : "
        << header_.numKeys
        << "\n";

    std::cout
        << "First leaf     : "
        << header_.firstLeaf
        << "\n";

    std::cout
        << "n              : "
        << header_.n
        << "\n";
}

} // namespace bptree
