// Task 3: in-place deletion specialized for a suffix of the sorted keys.
// Remove entries from largest to smallest. Only left siblings are needed,
// because every node on the search path is its parent's rightmost child.
#include "bplustree.h"
#include "tree_build.h"

#include <stdexcept>
#include <vector>

namespace bptree {

void BPlusTree::eraseLargest(float expectedKey, const RecordId& expectedRid) {
    if (header_.numKeys == 0 || header_.numLevels == 0)
        throw std::runtime_error("eraseLargest: empty tree");

    std::vector<InternalNode> parents;
    BlockId id = header_.rootNode;
    for (std::uint32_t depth = 1; depth < header_.numLevels; ++depth) {
        InternalNode p = readInternal(id);
        if (p.numKeys == 0)
            throw std::runtime_error("eraseLargest: invalid one-child internal node");
        parents.push_back(p);
        id = p.children[p.numKeys];
    }
    LeafNode leaf = readLeaf(id);
    if (leaf.numKeys == 0) throw std::runtime_error("eraseLargest: empty leaf");
    const std::size_t last = leaf.numKeys - 1;
    if (leaf.keys[last] != expectedKey ||
        leaf.recordIds[last].block != expectedRid.block ||
        leaf.recordIds[last].slot != expectedRid.slot)
        throw std::runtime_error("eraseLargest: entry does not match greatest tree entry");
    --leaf.numKeys;
    --header_.numKeys;

    if (parents.empty() || leaf.numKeys >= minLeafKeys(n())) {
        writeLeaf(leaf);  // The root leaf is allowed to be empty.
        saveHeader();
        return;
    }

    InternalNode parent = parents.back();
    parents.pop_back();
    const std::size_t right = parent.numKeys;
    LeafNode left = readLeaf(parent.children[right - 1]);
    if (left.numKeys > minLeafKeys(n())) {
        // Borrow the largest entry of the left leaf; insert it at our front.
        for (std::size_t j = leaf.numKeys; j > 0; --j) {
            leaf.keys[j] = leaf.keys[j - 1];
            leaf.recordIds[j] = leaf.recordIds[j - 1];
        }
        --left.numKeys;
        leaf.keys[0] = left.keys[left.numKeys];
        leaf.recordIds[0] = left.recordIds[left.numKeys];
        ++leaf.numKeys;
        parent.keys[right - 1] = leaf.keys[0];
        writeLeaf(left);
        writeLeaf(leaf);
        writeInternal(parent);
        saveHeader();
        return;
    }

    // Merge the right leaf into the left leaf and unlink the retired block.
    for (std::size_t j = 0; j < leaf.numKeys; ++j) {
        left.keys[left.numKeys + j] = leaf.keys[j];
        left.recordIds[left.numKeys + j] = leaf.recordIds[j];
    }
    left.numKeys = static_cast<std::uint16_t>(left.numKeys + leaf.numKeys);
    left.nextLeaf = leaf.nextLeaf;
    writeLeaf(left);
    --header_.numNodes;
    --parent.numKeys;  // Drops the last separator and last child pointer.

    // Repair any internal underflow, working back towards the root.
    InternalNode node = parent;
    while (true) {
        if (parents.empty()) {
            if (node.numKeys == 0) {
                header_.rootNode = node.children[0];
                --header_.numLevels;
                --header_.numNodes;
            } else {
                writeInternal(node);
            }
            break;
        }
        if (node.numKeys + 1u >= minInternalChildren(n())) {
            writeInternal(node);
            break;
        }

        InternalNode grandparent = parents.back();
        parents.pop_back();
        const std::size_t r = grandparent.numKeys;
        InternalNode sibling = readInternal(grandparent.children[r - 1]);
        if (sibling.numKeys + 1u > minInternalChildren(n())) {
            // Rotate one child through the grandparent separator.
            for (std::size_t j = node.numKeys; j > 0; --j)
                node.keys[j] = node.keys[j - 1];
            for (std::size_t j = node.numKeys + 1u; j > 0; --j)
                node.children[j] = node.children[j - 1];
            node.keys[0] = grandparent.keys[r - 1];
            node.children[0] = sibling.children[sibling.numKeys];
            grandparent.keys[r - 1] = sibling.keys[sibling.numKeys - 1];
            --sibling.numKeys;
            ++node.numKeys;
            writeInternal(sibling);
            writeInternal(node);
            writeInternal(grandparent);
            break;
        }

        // Merge internal nodes; the separator becomes a key in the left node.
        const std::size_t base = sibling.numKeys;
        sibling.keys[base] = grandparent.keys[r - 1];
        for (std::size_t j = 0; j < node.numKeys; ++j)
            sibling.keys[base + 1 + j] = node.keys[j];
        for (std::size_t j = 0; j <= node.numKeys; ++j)
            sibling.children[base + 1 + j] = node.children[j];
        sibling.numKeys = static_cast<std::uint16_t>(base + 1 + node.numKeys);
        writeInternal(sibling);
        --header_.numNodes;
        --grandparent.numKeys;
        node = grandparent;
    }
    // firstLeaf stays unchanged: all merges keep the left node.
    // Retired blocks remain in the file but are not reachable tree nodes.
    saveHeader();
}

}  // namespace bptree
