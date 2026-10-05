#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include "../../Storage/src/config.h"
#include "../../Storage/src/disk.h"
#include "../../Storage/src/record.h"

namespace bptree {

using storage::BlockId;
using storage::RecordId;

// ============================================================
// B+ TREE CONFIGURATION
// ============================================================

// Every B+ tree node occupies exactly one 4096-byte block.
constexpr std::size_t NODE_SIZE = storage::BLOCK_SIZE;

// ------------------------------------------------------------
// Leaf entry
//
// FG_PCT_home = 4 bytes
// RecordId     = 6 bytes
// ------------------------------------------------------------

constexpr std::size_t KEY_SIZE = sizeof(float);
constexpr std::size_t LEAF_ENTRY_SIZE = KEY_SIZE + storage::RECORD_ID_SIZE;

// ------------------------------------------------------------
// Leaf node header
//
// type       = 1 byte
// numKeys    = 2 bytes
// nodeId     = 4 bytes
// nextLeaf   = 4 bytes
//
// Total = 11 bytes
// ------------------------------------------------------------

constexpr std::size_t LEAF_HEADER_SIZE = 11;

// n = maximum number of keys in a node.
// We derive it from the leaf-node capacity because the course
// convention uses the same n for leaf and internal nodes.
constexpr std::size_t BPLUS_N = (NODE_SIZE - LEAF_HEADER_SIZE) / LEAF_ENTRY_SIZE;

// ------------------------------------------------------------
// Internal node layout
//
// header = 7 bytes
// n keys = n * 4
// n+1 children = (n+1) * 4
// ------------------------------------------------------------

constexpr std::size_t INTERNAL_HEADER_SIZE = 7;

constexpr std::size_t INTERNAL_NODE_SIZE = INTERNAL_HEADER_SIZE + BPLUS_N * KEY_SIZE + (BPLUS_N + 1) * sizeof(BlockId);

static_assert(BPLUS_N == 408, "Unexpected B+ tree n; check block and key sizes.");

static_assert(INTERNAL_NODE_SIZE <= NODE_SIZE, "Internal node does not fit in one block.");


// ============================================================
// NODE TYPES
// ============================================================

enum class NodeType : std::uint8_t {
    LEAF = 1,
    INTERNAL = 2
};


// ============================================================
// LEAF NODE
// ============================================================

struct LeafNode {
    BlockId nodeId = 0;

    std::uint16_t numKeys = 0;

    BlockId nextLeaf = 0;

    float keys[BPLUS_N]{};

    RecordId recordIds[BPLUS_N]{};
};


// ============================================================
// INTERNAL NODE
// ============================================================

struct InternalNode {
    BlockId nodeId = 0;

    std::uint16_t numKeys = 0;

    float keys[BPLUS_N]{};

    BlockId children[BPLUS_N + 1]{};
};


// ============================================================
// B+ TREE HEADER
//
// Stored in block 0 of index.db
// ============================================================

struct BPlusTreeHeader {
    BlockId rootNode = 0;

    std::uint32_t numNodes = 0;

    std::uint32_t numLevels = 0;

    std::uint32_t numKeys = 0;

    BlockId firstLeaf = 0;

    std::uint32_t n = BPLUS_N;
};


// ============================================================
// B+ TREE CLASS
// ============================================================

class BPlusTree {

public:

    // Create a new empty B+ tree.
    // indexPath = path to index.db
    explicit BPlusTree(const std::string& indexPath, bool createNew = true);

    // --------------------------------------------------------
    // Header / metadata
    // --------------------------------------------------------

    const BPlusTreeHeader& header() const {
        return header_;
    }

    std::uint32_t n() const {
        return header_.n;
    }

    std::uint32_t numNodes() const {
        return header_.numNodes;
    }

    std::uint32_t numLevels() const {
        return header_.numLevels;
    }

    BlockId rootNode() const {
        return header_.rootNode;
    }

    // --------------------------------------------------------
    // Disk access
    // --------------------------------------------------------

    storage::Disk& disk() {
        return indexDisk_;
    }

    // --------------------------------------------------------
    // Node operations
    // --------------------------------------------------------

    LeafNode readLeaf(BlockId nodeId);

    void writeLeaf(const LeafNode& node);

    InternalNode readInternal(BlockId nodeId);

    void writeInternal(const InternalNode& node);

    // --------------------------------------------------------
    // Allocation
    // --------------------------------------------------------

    BlockId allocateLeaf();

    BlockId allocateInternal();

    // --------------------------------------------------------
    // Statistics / display
    // --------------------------------------------------------

    void printDesign() const;

    void printHeader() const;

    // Save header to block 0.
    void saveHeader();

    private:

    storage::Disk indexDisk_;

    BPlusTreeHeader header_{};

    void initializeNewTree();

    void loadHeader();

    void encodeHeader(std::uint8_t* buffer) const;

    void decodeHeader(const std::uint8_t* buffer);

    void encodeLeaf(const LeafNode& node, std::uint8_t* buffer) const;

    LeafNode decodeLeaf(const std::uint8_t* buffer) const;

    void encodeInternal(const InternalNode& node, std::uint8_t* buffer) const;

    InternalNode decodeInternal(const std::uint8_t* buffer) const;
};

} // namespace bptree