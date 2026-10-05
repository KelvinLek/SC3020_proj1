// bulk_load.cpp - builds the B+ tree bottom-up: sort the entries, fill the leaves, then build each level of internal nodes until one node (the root) is left.
#include <algorithm>
#include <utility>

#include "tree_build.h"

namespace bptree {

namespace {

// Split total items into groups of at most maxPer. If the last group is below minPer, move some items into it from the group before.
std::vector<std::size_t> groupSizes(std::size_t total, std::size_t maxPer, std::size_t minPer) {
    std::vector<std::size_t> sizes;
    for (std::size_t left = total; left > 0; left -= sizes.back())
        sizes.push_back(std::min(left, maxPer));
    if (sizes.size() > 1 && sizes.back() < minPer) {
        std::size_t moved = minPer - sizes.back();
        sizes[sizes.size() - 2] -= moved;
        sizes.back() += moved;
    }
    return sizes;
}

}  // namespace

void bulkLoad(BPlusTree& tree, std::vector<Entry> entries) {
    const std::size_t n = tree.n();

    std::sort(entries.begin(), entries.end(), entryLess);

    if (entries.empty()) {
        BlockId leaf = tree.allocateLeaf();
        tree.setTreeInfo(leaf, 1, 0, leaf);
        return;
    }

    struct Built { BlockId id; float minKey; };
    std::vector<Built> level;

    // leaves - allocate all ids first so each leaf knows the next one
    std::vector<std::size_t> leafSizes = groupSizes(entries.size(), n, minLeafKeys(n));
    std::vector<BlockId> leafIds;
    for (std::size_t i = 0; i < leafSizes.size(); ++i) leafIds.push_back(tree.allocateLeaf());

    std::size_t pos = 0;
    for (std::size_t i = 0; i < leafSizes.size(); ++i) {
        LeafNode leaf;
        leaf.nodeId = leafIds[i];
        leaf.numKeys = static_cast<std::uint16_t>(leafSizes[i]);
        for (std::size_t j = 0; j < leafSizes[i]; ++j, ++pos) {
            leaf.keys[j] = entries[pos].key;
            leaf.recordIds[j] = entries[pos].rid;
        }
        leaf.nextLeaf = (i + 1 < leafIds.size()) ? leafIds[i + 1] : NO_NEXT_LEAF;
        tree.writeLeaf(leaf);
        level.push_back({leaf.nodeId, leaf.keys[0]});
    }

    // internal levels - a parent's keys are the smallest key of each child except the first
    std::uint32_t levels = 1;
    while (level.size() > 1) {
        std::vector<Built> parents;
        std::vector<std::size_t> sizes = groupSizes(level.size(), n + 1, minInternalChildren(n));
        std::size_t c = 0;
        for (std::size_t size : sizes) {
            InternalNode parent;
            parent.nodeId = tree.allocateInternal();
            parent.numKeys = static_cast<std::uint16_t>(size - 1);
            float minKey = level[c].minKey;
            for (std::size_t j = 0; j < size; ++j, ++c) {
                parent.children[j] = level[c].id;
                if (j > 0) parent.keys[j - 1] = level[c].minKey;
            }
            tree.writeInternal(parent);
            parents.push_back({parent.nodeId, minKey});
        }
        level = std::move(parents);
        ++levels;
    }

    tree.setTreeInfo(level.front().id, levels, static_cast<std::uint32_t>(entries.size()),
                     leafIds.front());
}

}  // namespace bptree
