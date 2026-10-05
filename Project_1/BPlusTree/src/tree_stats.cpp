// tree_stats.cpp - statistics, validation and printing for the B+ tree.
// A node at depth numLevels() is a leaf, anything above is internal.
#include <algorithm>
#include <functional>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "tree_build.h"

namespace bptree {

bool entryLess(const Entry& a, const Entry& b) {
    if (a.key != b.key) return a.key < b.key;
    if (a.rid.block != b.rid.block) return a.rid.block < b.rid.block;
    return a.rid.slot < b.rid.slot;
}

TreeStats treeStats(BPlusTree& tree) {
    TreeStats s;
    s.n = tree.n();
    if (tree.numLevels() == 0) return s;
    std::vector<BlockId> level{tree.rootNode()};
    for (std::uint32_t depth = 1; depth <= tree.numLevels(); ++depth) {
        ++s.levels;
        std::vector<BlockId> below;
        for (BlockId id : level) {
            ++s.numNodes;
            if (depth == tree.numLevels()) {
                LeafNode leaf = tree.readLeaf(id);
                if (id == tree.rootNode()) s.rootKeys.assign(leaf.keys, leaf.keys + leaf.numKeys);
                ++s.numLeaves;
                s.numEntries += leaf.numKeys;
            } else {
                InternalNode node = tree.readInternal(id);
                if (id == tree.rootNode()) s.rootKeys.assign(node.keys, node.keys + node.numKeys);
                ++s.numInternal;
                below.insert(below.end(), node.children, node.children + node.numKeys + 1);
            }
        }
        level = std::move(below);
    }
    return s;
}

std::string validate(BPlusTree& tree, std::vector<Entry> expected) {
    std::ostringstream err;
    const std::size_t n = tree.n();
    const std::uint32_t levels = tree.numLevels();
    if (levels == 0) return "tree has no root";

    std::size_t nodesSeen = 0;
    std::vector<BlockId> leavesInOrder;

    // checks the subtree at id: sorted, within [lo, hi], not over/under full
    std::function<bool(BlockId, std::uint32_t, float, float)> check =
        [&](BlockId id, std::uint32_t depth, float lo, float hi) -> bool {
        ++nodesSeen;
        bool isRoot = (id == tree.rootNode());
        const float* keys;
        std::size_t numKeys;
        LeafNode leaf;
        InternalNode node;
        if (depth == levels) {
            leaf = tree.readLeaf(id);   // throws if not a leaf
            keys = leaf.keys;
            numKeys = leaf.numKeys;
        } else {
            node = tree.readInternal(id);
            keys = node.keys;
            numKeys = node.numKeys;
        }
        if (!std::is_sorted(keys, keys + numKeys)) {
            err << "node " << id << ": keys not sorted";
            return false;
        }
        if (numKeys > 0 && (keys[0] < lo || keys[numKeys - 1] > hi)) {
            err << "node " << id << ": key outside the range allowed by its parent";
            return false;
        }
        if (numKeys > n) {
            err << "node " << id << ": " << numKeys << " keys > n=" << n;
            return false;
        }

        if (depth == levels) {
            if (!isRoot && numKeys < minLeafKeys(n)) {
                err << "leaf " << id << ": underfull (" << numKeys << " keys)";
                return false;
            }
            leavesInOrder.push_back(id);
            return true;
        }

        std::size_t numChildren = numKeys + 1;
        std::size_t minChildren = isRoot ? 2 : minInternalChildren(n);
        if (numChildren < minChildren) {
            err << "internal " << id << ": underfull (" << numChildren << " children)";
            return false;
        }
        // inclusive on both ends because duplicate keys can span two children
        for (std::size_t i = 0; i < numChildren; ++i) {
            float cLo = (i == 0) ? lo : keys[i - 1];
            float cHi = (i == numKeys) ? hi : keys[i];
            if (!check(node.children[i], depth + 1, cLo, cHi)) return false;
        }
        return true;
    };

    try {
        const float inf = std::numeric_limits<float>::infinity();
        if (!check(tree.rootNode(), 1, -inf, inf)) return err.str();

        if (tree.header().firstLeaf != leavesInOrder.front()) return "header: wrong first leaf";
        if (tree.numNodes() != nodesSeen) {
            err << "header: numNodes=" << tree.numNodes() << " but " << nodesSeen << " reachable";
            return err.str();
        }

        // leaf chain should match the sorted input exactly
        std::sort(expected.begin(), expected.end(), entryLess);
        std::size_t i = 0, leafNo = 0;
        for (BlockId id = tree.header().firstLeaf; id != NO_NEXT_LEAF; ++leafNo) {
            if (leafNo >= leavesInOrder.size() || leavesInOrder[leafNo] != id)
                return "leaf chain does not follow the leaves left to right";
            LeafNode leaf = tree.readLeaf(id);
            for (std::size_t j = 0; j < leaf.numKeys; ++j, ++i) {
                if (i >= expected.size() || leaf.keys[j] != expected[i].key ||
                    leaf.recordIds[j].block != expected[i].rid.block ||
                    leaf.recordIds[j].slot != expected[i].rid.slot) {
                    err << "leaf chain differs from the sorted input at entry " << i;
                    return err.str();
                }
            }
            id = leaf.nextLeaf;
        }
        if (leafNo != leavesInOrder.size()) return "leaf chain ends early";
        if (i != expected.size() || tree.header().numKeys != i) {
            err << "leaf chain has " << i << " entries, header says " << tree.header().numKeys
                << ", expected " << expected.size();
            return err.str();
        }
    } catch (const std::exception& e) {
        return e.what();
    }
    return "";
}

std::string dumpTree(BPlusTree& tree, std::size_t maxNodesPerLevel) {
    std::ostringstream out;
    if (tree.numLevels() == 0) return "(no tree)\n";
    std::vector<BlockId> level{tree.rootNode()};
    for (std::uint32_t depth = 1; depth <= tree.numLevels(); ++depth) {
        out << "level " << depth << " (" << level.size() << " nodes):";
        std::vector<BlockId> below;
        for (std::size_t k = 0; k < level.size(); ++k) {
            const bool isLeaf = (depth == tree.numLevels());
            if (k >= maxNodesPerLevel && isLeaf) break;
            LeafNode leaf;
            InternalNode node;
            const float* keys;
            std::size_t numKeys;
            if (isLeaf) {
                leaf = tree.readLeaf(level[k]);
                keys = leaf.keys;
                numKeys = leaf.numKeys;
            } else {
                node = tree.readInternal(level[k]);
                keys = node.keys;
                numKeys = node.numKeys;
                below.insert(below.end(), node.children, node.children + numKeys + 1);
            }
            if (k < maxNodesPerLevel) {
                out << " [";
                for (std::size_t j = 0; j < numKeys; ++j) out << (j ? " " : "") << keys[j];
                out << "]";
            }
        }
        if (level.size() > maxNodesPerLevel) out << " ...";
        out << "\n";
        level = std::move(below);
    }
    return out.str();
}

}  // namespace bptree
