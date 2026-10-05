#include "bplustree.h"
#include "bplustree_bytes.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace bptree {

using namespace detail;

// ============================================================
// Constructor
// ============================================================

BPlusTree::BPlusTree(const std::string& indexPath,
                     bool createNew)
    : indexDisk_(indexPath, createNew) {

    if (createNew) {
        initializeNewTree();
    } else {
        loadHeader();
    }
}


// ============================================================
// Initialize new empty tree
// ============================================================

void BPlusTree::initializeNewTree() {

    // Block 0 = B+ tree header.
    indexDisk_.allocateBlock();

    header_.rootNode = 0;
    header_.numNodes = 0;
    header_.numLevels = 0;
    header_.numKeys = 0;
    header_.firstLeaf = 0;
    header_.n = static_cast<std::uint32_t>(BPLUS_N);

    saveHeader();
}


// ============================================================
// Header encoding
// ============================================================

void BPlusTree::encodeHeader(std::uint8_t* buffer) const {

    std::fill(
        buffer,
        buffer + NODE_SIZE,
        static_cast<std::uint8_t>(0)
    );

    // Magic number: "BPTREE1"
    buffer[0] = 'B';
    buffer[1] = 'P';
    buffer[2] = 'T';
    buffer[3] = 'R';
    buffer[4] = 'E';
    buffer[5] = 'E';
    buffer[6] = '1';

    // Version
    putU32(buffer + 8, 1);

    // rootNode
    putU32(buffer + 12, header_.rootNode);

    // number of nodes
    putU32(buffer + 16, header_.numNodes);

    // number of levels
    putU32(buffer + 20, header_.numLevels);

    // total number of indexed keys
    putU32(buffer + 24, header_.numKeys);

    // first leaf
    putU32(buffer + 28, header_.firstLeaf);

    // n
    putU32(buffer + 32, header_.n);
}


// ============================================================
// Header decoding
// ============================================================

void BPlusTree::decodeHeader(const std::uint8_t* buffer) {

    if (buffer[0] != 'B' ||
        buffer[1] != 'P' ||
        buffer[2] != 'T' ||
        buffer[3] != 'R' ||
        buffer[4] != 'E' ||
        buffer[5] != 'E' ||
        buffer[6] != '1') {

        throw std::runtime_error(
            "BPlusTree: invalid index.db header"
        );
    }

    header_.rootNode = getU32(buffer + 12);
    header_.numNodes = getU32(buffer + 16);
    header_.numLevels = getU32(buffer + 20);
    header_.numKeys = getU32(buffer + 24);
    header_.firstLeaf = getU32(buffer + 28);
    header_.n = getU32(buffer + 32);

    if (header_.n != BPLUS_N) {
        throw std::runtime_error(
            "BPlusTree: index was created with a different n"
        );
    }
}


// ============================================================
// Save header
// ============================================================

void BPlusTree::saveHeader() {

    std::vector<std::uint8_t> buffer(NODE_SIZE, 0);

    encodeHeader(buffer.data());

    indexDisk_.writeBlock(
        storage::HEADER_BLOCK_ID,
        buffer.data()
    );
}


// ============================================================
// Load header
// ============================================================

void BPlusTree::loadHeader() {

    std::vector<std::uint8_t> buffer(NODE_SIZE);

    indexDisk_.readBlock(
        storage::HEADER_BLOCK_ID,
        buffer.data()
    );

    decodeHeader(buffer.data());
}

} // namespace bptree
