#include "bplustree.h"
#include "bplustree_bytes.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace bptree {

using namespace detail;

// ============================================================
// Allocate internal node
// ============================================================

BlockId BPlusTree::allocateInternal() {

    InternalNode node;

    BlockId id = indexDisk_.allocateBlock();

    node.nodeId = id;
    node.numKeys = 0;

    std::fill(
        std::begin(node.children),
        std::end(node.children),
        static_cast<BlockId>(0)
    );

    writeInternal(node);

    ++header_.numNodes;

    saveHeader();

    return id;
}


// ============================================================
// Encode internal node
// ============================================================

void BPlusTree::encodeInternal(
    const InternalNode& node,
    std::uint8_t* buffer
) const {

    std::fill(
        buffer,
        buffer + NODE_SIZE,
        static_cast<std::uint8_t>(0)
    );

    // Byte 0 = internal node
    buffer[0] =
        static_cast<std::uint8_t>(
            NodeType::INTERNAL
        );

    // Bytes 1-2 = number of keys
    putU16(
        buffer + 1,
        node.numKeys
    );

    // Bytes 3-6 = node ID
    putU32(
        buffer + 3,
        node.nodeId
    );

    // Keys begin at byte 7.
    std::size_t offset =
        INTERNAL_HEADER_SIZE;

    for (std::size_t i = 0;
         i < node.numKeys;
         ++i) {

        putFloat(
            buffer + offset,
            node.keys[i]
        );

        offset += sizeof(float);
    }

    // Children follow the keys.
    for (std::size_t i = 0;
         i <= node.numKeys;
         ++i) {

        putU32(
            buffer + offset,
            node.children[i]
        );

        offset += sizeof(BlockId);
    }
}


// ============================================================
// Decode internal node
// ============================================================

InternalNode BPlusTree::decodeInternal(
    const std::uint8_t* buffer
) const {

    if (buffer[0] !=
        static_cast<std::uint8_t>(
            NodeType::INTERNAL
        )) {

        throw std::runtime_error(
            "BPlusTree: block is not an internal node"
        );
    }

    InternalNode node;

    node.numKeys =
        getU16(buffer + 1);

    node.nodeId =
        getU32(buffer + 3);

    if (node.numKeys > BPLUS_N) {
        throw std::runtime_error(
            "BPlusTree: invalid internal key count"
        );
    }

    std::size_t offset =
        INTERNAL_HEADER_SIZE;

    for (std::size_t i = 0;
         i < node.numKeys;
         ++i) {

        node.keys[i] =
            getFloat(buffer + offset);

        offset += sizeof(float);
    }

    for (std::size_t i = 0;
         i <= node.numKeys;
         ++i) {

        node.children[i] =
            getU32(buffer + offset);

        offset += sizeof(BlockId);
    }

    return node;
}


// ============================================================
// Write internal
// ============================================================

void BPlusTree::writeInternal(
    const InternalNode& node
) {

    if (node.numKeys > BPLUS_N) {
        throw std::runtime_error(
            "BPlusTree: internal node exceeds n"
        );
    }

    std::vector<std::uint8_t> buffer(
        NODE_SIZE,
        0
    );

    encodeInternal(
        node,
        buffer.data()
    );

    indexDisk_.writeBlock(
        node.nodeId,
        buffer.data()
    );
}


// ============================================================
// Read internal
// ============================================================

InternalNode BPlusTree::readInternal(
    BlockId nodeId
) {

    std::vector<std::uint8_t> buffer(
        NODE_SIZE
    );

    indexDisk_.readBlock(
        nodeId,
        buffer.data()
    );

    return decodeInternal(
        buffer.data()
    );
}

} // namespace bptree
