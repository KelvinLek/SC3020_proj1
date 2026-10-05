#include "bplustree.h"
#include "bplustree_bytes.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace bptree {

using namespace detail;

// ============================================================
// Allocate leaf
// ============================================================

BlockId BPlusTree::allocateLeaf() {

    LeafNode node;

    BlockId id = indexDisk_.allocateBlock();

    node.nodeId = id;
    node.numKeys = 0;
    node.nextLeaf = 0;

    writeLeaf(node);

    ++header_.numNodes;

    if (header_.numLevels == 0) {
        header_.rootNode = id;
        header_.firstLeaf = id;
        header_.numLevels = 1;
    }

    saveHeader();

    return id;
}


// ============================================================
// Encode leaf
// ============================================================

void BPlusTree::encodeLeaf(
    const LeafNode& node,
    std::uint8_t* buffer
) const {

    std::fill(
        buffer,
        buffer + NODE_SIZE,
        static_cast<std::uint8_t>(0)
    );

    // Byte 0: node type
    buffer[0] = static_cast<std::uint8_t>(NodeType::LEAF);

    // Bytes 1-2: number of keys
    putU16(buffer + 1, node.numKeys);

    // Bytes 3-6: node ID
    putU32(buffer + 3, node.nodeId);

    // Bytes 7-10: next leaf
    putU32(buffer + 7, node.nextLeaf);

    // Entries begin at byte 11.
    std::size_t offset = LEAF_HEADER_SIZE;

    for (std::size_t i = 0; i < node.numKeys; ++i) {

        // Key
        putFloat(
            buffer + offset,
            node.keys[i]
        );

        offset += 4;

        // RecordId
        std::uint8_t ridBuffer[storage::RECORD_ID_SIZE];

        storage::encodeRecordId(
            node.recordIds[i],
            ridBuffer
        );

        std::memcpy(
            buffer + offset,
            ridBuffer,
            storage::RECORD_ID_SIZE
        );

        offset += storage::RECORD_ID_SIZE;
    }
}


// ============================================================
// Decode leaf
// ============================================================

LeafNode BPlusTree::decodeLeaf(
    const std::uint8_t* buffer
) const {

    if (buffer[0] !=
        static_cast<std::uint8_t>(NodeType::LEAF)) {

        throw std::runtime_error(
            "BPlusTree: block is not a leaf node"
        );
    }

    LeafNode node;

    node.numKeys = getU16(buffer + 1);
    node.nodeId = getU32(buffer + 3);
    node.nextLeaf = getU32(buffer + 7);

    if (node.numKeys > BPLUS_N) {
        throw std::runtime_error(
            "BPlusTree: invalid leaf key count"
        );
    }

    std::size_t offset = LEAF_HEADER_SIZE;

    for (std::size_t i = 0; i < node.numKeys; ++i) {

        node.keys[i] = getFloat(buffer + offset);

        offset += 4;

        node.recordIds[i] =
            storage::decodeRecordId(buffer + offset);

        offset += storage::RECORD_ID_SIZE;
    }

    return node;
}


// ============================================================
// Write leaf
// ============================================================

void BPlusTree::writeLeaf(const LeafNode& node) {

    if (node.numKeys > BPLUS_N) {
        throw std::runtime_error(
            "BPlusTree: leaf node exceeds n"
        );
    }

    std::vector<std::uint8_t> buffer(
        NODE_SIZE,
        0
    );

    encodeLeaf(
        node,
        buffer.data()
    );

    indexDisk_.writeBlock(
        node.nodeId,
        buffer.data()
    );
}


// ============================================================
// Read leaf
// ============================================================

LeafNode BPlusTree::readLeaf(BlockId nodeId) {

    std::vector<std::uint8_t> buffer(
        NODE_SIZE
    );

    indexDisk_.readBlock(
        nodeId,
        buffer.data()
    );

    return decodeLeaf(
        buffer.data()
    );
}

} // namespace bptree
