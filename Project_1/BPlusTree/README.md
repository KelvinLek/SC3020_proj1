# B+ tree index (Task 2) - Members 2 and 3

A B+ tree on `FG_PCT_home`, stored on disk in `index.db` (one node per 4096-byte block), over the records that the Storage module keeps in `../Storage/data.db`.

- **Member 2 - node design and storage layer:** node formats, parameter `n`, allocating/reading/writing nodes in `index.db` (`bplustree*.{h,cpp}`, `main.cpp`).
- **Member 3 - construction:** bulk loading the NBA data into the tree, the Task 2 statistics, and validation (`tree_build.h`, `bulk_load.cpp`, `tree_stats.cpp`, `task2.cpp`).

## Install and run

Needs a C++17 compiler, and nothing else.

First build `../Storage/data.db` with Task 1 (`../Storage/README.md`). Then, from `Project_1/BPlusTree`:

```bash
g++ -std=c++17 -O2 -I../Storage/src -o task2 src/task2.cpp src/bulk_load.cpp src/tree_stats.cpp src/bplustree_header_io.cpp src/bplustree_leaf.cpp src/bplustree_internal.cpp src/bplustree_debug.cpp ../Storage/src/disk.cpp ../Storage/src/record.cpp ../Storage/src/storage.cpp

./task2                             # Windows: task2.exe
./task2 --limit 1000 --print        # small tree, prints the keys of every level (demo)
```

Or `make run`. Options: `task2 [data.db] [--index index.db] [--limit K] [--print]`. `task2` overwrites `index.db`, then reopens it, so every statistic is read back from disk.

To create an empty `index.db`, print the node layout: `make bplustree && ./bplustree`.

## Task 2 results

| Statistic | Value |
|---|---|
| Parameter n | 408 |
| Number of nodes | 67 (1 internal + 66 leaves) |
| Number of levels | 2 |
| Content of the root node | 65 keys: 0.342 0.358 0.368 ... 0.500 0.500 ... 0.584 0.602 (full list printed by `task2`) |
| Records indexed | 26552 (99 records with an empty `FG_PCT_home` are not indexed) |
| `index.db` | 68 blocks = 1 header + 67 nodes (278528 bytes) |

## File layout

```text
BPlusTree/
├── Makefile
└── src/
    ├── bplustree.h              Class + struct declarations, node layout constants        (Member 2)
    ├── bplustree_bytes.h        Shared little-endian encode/decode helpers (u16/u32/float) (Member 2)
    ├── bplustree_header_io.cpp  Ctor, initializeNewTree, header encode/decode, save/load    (Member 2)
    ├── bplustree_leaf.cpp       Leaf node: allocate, encode/decode, read/write             (Member 2)
    ├── bplustree_internal.cpp   Internal node: allocate, encode/decode, read/write         (Member 2)
    ├── bplustree_debug.cpp      printDesign() / printHeader() diagnostics                  (Member 2)
    ├── main.cpp                 Demo: creates an empty index.db, prints design + header    (Member 2)
    ├── tree_build.h             Entry, bulkLoad, treeStats, validate, dumpTree             (Member 3)
    ├── bulk_load.cpp            bulkLoad: sort -> pack leaves -> build parent levels       (Member 3)
    ├── tree_stats.cpp           treeStats, validate, dumpTree (all read from index.db)     (Member 3)
    └── task2.cpp                Task 2 driver: scan data.db, build index.db, report        (Member 3)
```

## Construction (Member 3)

The tree is built by **bulk loading** (`bulk_load.cpp`), bottom-up:

1. Read every data block of `data.db` once and collect `(FG_PCT_home, RecordId)` for each live record with a non-empty key.
2. Sort by key. Ties are broken by `RecordId`, so the same data always gives the same tree.
3. Pack the entries left to right into leaves of `n` entries, and link each leaf to the next one (`nextLeaf`).
4. Build each level above from the level below: a parent holds up to `n + 1` children, and its keys are the smallest key of every child except the first. Repeat until one node is left: the root.
5. Record the root, number of levels, number of keys and first leaf in the header (block 0) with `setTreeInfo`.

Nodes are packed full. Only the last node of a level may be short; if it would fall below the minimum fill (leaf `floor((n+1)/2)` keys, internal `floor(n/2)+1` children), entries are moved into it from its left neighbour.

**Duplicate keys.** Each record is its own leaf entry, so equal keys can run across leaves (848 games have `FG_PCT_home = 0.500`, and the root contains 0.500 twice). A separator therefore satisfies *left subtree <= key <= right subtree*. A search for `> 0.5` must look for the first key strictly greater than 0.5 and follow the leaf chain from there.

**Which nodes are leaves.** All leaves are on the bottom level, so a node read at depth `numLevels()` is a leaf and any other node is internal. `readLeaf`/`readInternal` throw if a block holds the other type.

**Validation** (`validate`, run at the end of every `task2`): keys sorted in every node, keys within the range their parent allows, no node over `n` keys or under the minimum fill, all leaves on the same level, the header's node/key counts and first leaf match the tree, and walking the leaf chain gives exactly the sorted input.

**Block writes during the build.** `allocateLeaf`/`allocateInternal` write the new empty node and the header, then the loader writes the filled node, so the full build does about 4 block writes per node (269 in total). Reads during Task 3 are not affected.

---

## Node format reference (Member 2)

Every node is exactly one 4096-byte block (`storage::BLOCK_SIZE`).

**Leaf node** (`LEAF_HEADER_SIZE` = 11 bytes header):

| Bytes | Field                                                    |
| ----- | -------------------------------------------------------- |
| 0     | node type (`0x01` = LEAF)                                |
| 1–2   | numKeys (u16)                                            |
| 3–6   | nodeId (u32)                                             |
| 7–10  | nextLeaf (u32), 0 = no next leaf                         |
| 11..  | `numKeys` × (4-byte float key + 6-byte RecordId) entries |

**Internal node** (`INTERNAL_HEADER_SIZE` = 7 bytes header):

| Bytes | Field                                                                     |
| ----- | ------------------------------------------------------------------------- |
| 0     | node type (`0x02` = INTERNAL)                                             |
| 1–2   | numKeys (u16)                                                             |
| 3–6   | nodeId (u32)                                                              |
| 7..   | `numKeys` × 4-byte float keys, then `numKeys + 1` × 4-byte child BlockIds |

**B+ tree order (`n`)** is derived from how many leaf entries fit in one block:

```text
BPLUS_N = floor((4096 - 11) / 10)
        = 408
```

The calculation uses:

```text
Key size       = 4 bytes
RecordId size  = 6 bytes
Leaf entry     = 10 bytes
Leaf header    = 11 bytes
```

The project/course convention uses `n` as the maximum number of keys in a node, so:

```text
Maximum leaf keys      = 408
Maximum internal keys  = 408
Maximum child pointers = 409
```

For the internal node:

```text
7 + (408 × 4) + (409 × 4)
= 3275 bytes
```

which fits within the 4096-byte block.

Block 0 of `index.db` is reserved for the `BPlusTreeHeader` (`rootNode`, `numNodes`, `numLevels`, `numKeys`, `firstLeaf`, `n`), tagged with a `"BPTREE1"` magic number so `decodeHeader` can detect a corrupt or foreign file.

## BPlusTree API (Member 2)

```cpp
BPlusTree tree("index.db", /*createNew=*/false);   // open the index built by task2

BlockId allocateLeaf();
BlockId allocateInternal();

LeafNode readLeaf(BlockId nodeId);            // 1 block read
InternalNode readInternal(BlockId nodeId);    // 1 block read

void writeLeaf(const LeafNode& node);         // 1 block write
void writeInternal(const InternalNode& node); // 1 block write

BlockId rootNode() const;
uint32_t numNodes() const;
uint32_t numLevels() const;
uint32_t n() const;
const BPlusTreeHeader& header() const;        // also numKeys, firstLeaf

// Added by Member 3: set root, levels, key count and first leaf, then save block 0.
void setTreeInfo(BlockId root, uint32_t numLevels, uint32_t numKeys, BlockId firstLeaf);

storage::Disk& disk();                        // reads()/writes() count index block accesses
void saveHeader();
```

A `LeafNode` contains the leaf `keys`, corresponding `recordIds`, `numKeys`, and `nextLeaf`. An `InternalNode` contains the internal `keys`, `children`, `numKeys`, and `nodeId`.

**For Member 4 (Task 3):** opening the tree reads the header (1 block read), so call `tree.disk().resetCounters()` after opening and before the search. Then `tree.disk().reads()` is the number of index nodes accessed.

## Reference values

```text
Block size                  = 4096 bytes
Key                         = FG_PCT_home (float)
Key size                    = 4 bytes
RecordId size               = 6 bytes
Leaf entry size             = 10 bytes
Leaf header size            = 11 bytes
Internal header size        = 7 bytes
Maximum keys per node (n)   = 408
Maximum internal pointers   = 409
Internal node size          = 3275 bytes
Unused leaf bytes           = 5 bytes
Header block                = Block 0
```
