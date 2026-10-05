# B+ Tree Index — Storage Layer

This is the on-disk storage layer for a B+ tree secondary index, keyed on a
`float` (the course dataset's `FG_PCT_home` column) and pointing to rows via
a `storage::RecordId`. It sits on top of the separate `Storage` module
(block allocation, raw disk I/O, `RecordId` encode/decode), which is not part
of this folder — see [Dependencies](#dependencies) below.

**What this layer currently does:** define the on-disk node formats, calculate
the B+ tree order `n`, and allocate / read / write individual leaf and internal
nodes as raw 4096-byte blocks.

---

## File layout

```text
BPlusTree/
├── index.db
└── src/
    ├── bplustree.h              Class + struct declarations, node layout constants
    ├── bplustree_bytes.h        Shared little-endian encode/decode helpers (u16/u32/float)
    ├── bplustree_header_io.cpp  Ctor, initializeNewTree, header encode/decode, save/load
    ├── bplustree_leaf.cpp       Leaf node: allocate, encode/decode, read/write
    ├── bplustree_internal.cpp   Internal node: allocate, encode/decode, read/write
    ├── bplustree_debug.cpp      printDesign() / printHeader() diagnostics
    └── main.cpp                 Demo entry point: creates index.db, prints design + header
```

---

## Dependencies

`bplustree.h` uses headers from the sibling `Storage` module:

```cpp
#include "config.h"
#include "disk.h"
#include "record.h"
```

The project is compiled with the `Storage/src` directory added to the include
path:

```text
-I../Storage/src
```

The expected project structure is:

```text
Project_1/
├── Storage/
│   ├── data.db
│   └── src/
│       ├── config.h       // storage::BLOCK_SIZE, storage::BlockId, storage::HEADER_BLOCK_ID
│       ├── disk.h         // storage::Disk (allocateBlock/readBlock/writeBlock)
│       ├── disk.cpp
│       ├── record.h       // storage::RecordId, encodeRecordId/decodeRecordId
│       └── record.cpp
└── BPlusTree/
    ├── index.db
    └── src/
        ├── bplustree.h
        ├── bplustree_bytes.h
        ├── bplustree_header_io.cpp
        ├── bplustree_leaf.cpp
        ├── bplustree_internal.cpp
        ├── bplustree_debug.cpp
        └── main.cpp
```

---

## Building

Run the command below from the `Project_1/BPlusTree` directory.

**Build (`main.cpp`):**

```bash
g++ -std=c++17 -O2 -I../Storage/src -o bplustree.exe \
src/main.cpp \
src/bplustree_header_io.cpp \
src/bplustree_leaf.cpp \
src/bplustree_internal.cpp \
src/bplustree_debug.cpp \
../Storage/src/disk.cpp \
../Storage/src/record.cpp

./bplustree.exe
```

Creates `index.db` in the `BPlusTree` directory, prints the derived node-layout
constants (`printDesign()`) and the current header (`printHeader()`).

> `main.cpp` creates/overwrites `index.db` in the current directory on every
> run because the tree is opened with `createNew = true`.

---

## Node format reference

Every node is exactly one 4096-byte block (`storage::BLOCK_SIZE`).

**Leaf node** (`LEAF_HEADER_SIZE` = 11 bytes header):

| Bytes | Field                                                    |
| ----- | -------------------------------------------------------- |
| 0     | node type (`0x01` = LEAF)                                |
| 1–2   | numKeys (u16)                                            |
| 3–6   | nodeId (u32)                                             |
| 7–10  | nextLeaf (u32)                                           |
| 11..  | `numKeys` × (4-byte float key + 6-byte RecordId) entries |

**Internal node** (`INTERNAL_HEADER_SIZE` = 7 bytes header):

| Bytes | Field                                                                     |
| ----- | ------------------------------------------------------------------------- |
| 0     | node type (`0x02` = INTERNAL)                                             |
| 1–2   | numKeys (u16)                                                             |
| 3–6   | nodeId (u32)                                                              |
| 7..   | `numKeys` × 4-byte float keys, then `numKeys + 1` × 4-byte child BlockIds |

**B+ tree order (`n`)** is derived from how many leaf entries fit in one
block:

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

The project/course convention uses `n` as the maximum number of keys in a
node, so:

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

Block 0 of `index.db` is reserved for the `BPlusTreeHeader`
(`rootNode`, `numNodes`, `numLevels`, `numKeys`, `firstLeaf`, `n`), tagged
with a `"BPTREE1"` magic number so `decodeHeader` can detect a corrupt or
foreign file.

---

## BPlusTree API

The `BPlusTree` class in `bplustree.h` provides the storage-layer API for
creating and accessing B+ tree nodes.

The main APIs available are:

```cpp
BlockId allocateLeaf();
BlockId allocateInternal();

LeafNode readLeaf(BlockId nodeId) const;
InternalNode readInternal(BlockId nodeId) const;

void writeLeaf(const LeafNode& node);
void writeInternal(const InternalNode& node);

BlockId rootNode() const;
uint32_t numNodes() const;
uint32_t numLevels() const;
uint32_t numKeys() const;
BlockId firstLeaf() const;
uint32_t n() const;

storage::Disk& disk();
void saveHeader();
```

A `LeafNode` contains the leaf `keys`, corresponding `recordIds`, `numKeys`,
and `nextLeaf`.

An `InternalNode` contains the internal `keys`, `children`, `numKeys`, and
`nodeId`.

For example, a leaf node can be accessed using:

```cpp
LeafNode leaf = tree.readLeaf(nodeId);
```

and written back using:

```cpp
tree.writeLeaf(leaf);
```

An internal node can similarly be accessed using:

```cpp
InternalNode internal = tree.readInternal(nodeId);
```

and written back using:

```cpp
tree.writeInternal(internal);
```

These APIs operate on individual B+ tree nodes stored as 4096-byte blocks.

---

## Current implementation

The storage-layer implementation provides:

- B+ tree node layout and constants
- Leaf and internal node structures
- B+ tree order calculation (`n = 408`)
- B+ tree header creation and I/O
- Leaf node allocation, encoding, decoding, reading and writing
- Internal node allocation, encoding, decoding, reading and writing
- Design and header diagnostics
- Public `BPlusTree` APIs for node allocation and block-level access

The current `main.cpp` initializes an empty B+ tree and prints its design and
header information. The current empty-tree header is:

```text
Root node      : 0
Number of nodes: 0
Number of levels: 0
Number of keys : 0
First leaf     : 0
n              : 408
```

The full indexed dataset is not constructed by this storage-layer demo.

---

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
