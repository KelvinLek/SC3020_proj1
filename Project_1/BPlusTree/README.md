# B+ tree indexing and deletion (Tasks 2 and 3) - Members 2, 3 and 4

A B+ tree on `FG_PCT_home`, stored on disk in `index.db` (one node per 4096-byte block), over the records that the Storage module keeps in `../Storage/data.db`.

- **Member 2 - node design and storage layer:** node formats, parameter `n`, allocating/reading/writing nodes in `index.db` (`bplustree*.{h,cpp}`, `main.cpp`).
- **Member 3 - construction:** bulk loading the NBA data into the tree, the Task 2 statistics, and validation (`tree_build.h`, `bulk_load.cpp`, `tree_stats.cpp`, `task2.cpp`).
- **Member 4 - retrieval and deletion:** indexed range retrieval, comparison with a linear scan, record deletion, B+ tree rebalancing, and updated statistics (`task3.cpp`, `bplustree_delete.cpp`).

For the complete Windows demonstration, see [Task 3: range retrieval and deletion](#task-3-range-retrieval-and-deletion-member-4) below.

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
    ├── task2.cpp                Task 2 driver: scan data.db, build index.db, report        (Member 3)
    ├── task3.cpp                Task 3 retrieval, deletion, comparison, and report        (Member 4)
    └── bplustree_delete.cpp     Remove greatest entry and rebalance the tree              (Member 4)
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

**For Member 4 (Task 3):** opening the tree reads the header (1 block read), so call `tree.disk().resetCounters()` after opening and before the search. Then `tree.disk().reads()` counts index-node read accesses, including repeated reads of the same node.

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

---

## Task 3: range retrieval and deletion (Member 4)

Task 3 deletes games with **`FG_PCT_home > 0.5`** using the B+ tree built in Task 2.
Games with `FG_PCT_home = 0.5` are retained. Records with a missing `FG_PCT_home`
are not indexed and are also retained.

The implementation uses the existing Storage component and updates the B+ tree
through deletion, borrowing, merging, and root contraction. It does not rebuild
the entire index after deletion.

### Task 3 files

Paths below are relative to `Project_1`.

| File | Purpose |
| --- | --- |
| `BPlusTree/src/task3.cpp` | Indexed retrieval, linear-scan comparison, deletion, timings, statistics, and validation |
| `BPlusTree/src/bplustree_delete.cpp` | Implements `BPlusTree::eraseLargest` and tree rebalancing |
| `BPlusTree/src/bplustree.h` | Adds the declaration of `eraseLargest` to the existing class |
| `build.sh` | Compiles Tasks 1, 2, and 3 separately using C++17 |
| `run_demo.sh` | Builds and runs all three tasks using `games.txt` |

### Build and run

Requirements: a C++17-capable `g++` compiler and Bash, configured for your operating system.

From the `Project_1` directory, compile all three programs:

```bash
bash build.sh
```

After Tasks 1 and 2 have generated the database and index, run Task 3:

```bash
./task3.exe Storage/data.db BPlusTree/index.db
```

To compile and run the complete demonstration from `games.txt`:

```bash
bash run_demo.sh
```

The demonstration regenerates its inputs under `demo_inputs`. Task 3 operates on copies and saves its results in a new `task3_runs/run_NNN` folder, preserving the original input files and previous results.

**Platform notes:** On Windows, run these commands in MSYS2 UCRT64. On macOS or Linux, use a terminal with Bash and a compatible compiler available. The build script names its executables with the `.exe` suffix on every platform; use the filenames shown above.

### Run Task 3 separately

After the first demonstration, run only Task 3 from **`Project_1`**:

```bash
./task3.exe demo_inputs/data.db demo_inputs/index.db
```

To use the group's existing Task 1 and Task 2 files:

```bash
./task3.exe Storage/data.db BPlusTree/index.db
```

Both files must come from the same database layout and contain the complete
Task 1/2 results. Do not use a Task 2 index built with `--limit`.

To rebuild the executables after editing source code:

```bash
bash build.sh
```

Usage:

```text
task3 [data.db] [index.db] [threshold]
```

The default paths are `Storage/data.db` and `BPlusTree/index.db`, relative to the
current working directory. The default threshold is `0.5`. For example:

```bash
./task3.exe demo_inputs/data.db demo_inputs/index.db 0.6
```

For a direct compilation from **`Project_1/BPlusTree`**, without the scripts:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -I../Storage/src -o task3.exe src/task3.cpp src/bplustree_delete.cpp src/bulk_load.cpp src/tree_stats.cpp src/bplustree_header_io.cpp src/bplustree_leaf.cpp src/bplustree_internal.cpp src/bplustree_debug.cpp ../Storage/src/disk.cpp ../Storage/src/record.cpp ../Storage/src/storage.cpp

./task3.exe ../Storage/data.db index.db
```

Here the explicit paths are necessary because the working directory differs.
The existing Makefile documents Task 2; use the scripts or direct command above
for Task 3. Do not compile `main.cpp`, `task1.cpp`, or `task2.cpp` into `task3.exe`,
as each defines a separate `main()` function.

### Retrieval and deletion process

1. **Create working copies.** Copy the input database and index into a fresh run
   folder so the original files remain available for repeatable experiments.
2. **Retrieve through the index.** Descend from the root using `upper_bound` to
   find the relevant subtree for keys strictly above the threshold. Follow the
   linked leaves and collect matching keys and `RecordId` values. Duplicate keys
   may span multiple leaves.
3. **Read matching records.** Group matching addresses by data block, then read
   each affected block once. Check the stored records against the index and
   calculate their average `FG_PCT_home` before deletion.
4. **Perform the comparison scan.** Scan all data blocks before any deletion.
   Require the linear scan and indexed retrieval to return the same matches.
   Validate the complete index against all live, non-NULL-key records.
5. **Delete from the index.** Process matching entries from largest to smallest.
   `eraseLargest` checks both the expected key and record address, removes the
   entry, and repairs any underflow.
6. **Delete from storage.** Call the existing `StorageManager::deleteRecords` to
   mark matching records with tombstones. This reads and writes each affected
   data block once, without moving surviving records or changing their addresses.
7. **Reopen and verify.** Close and reopen the updated files. Check that no
   qualifying live record remains, validate the tree against storage, and report
   the updated node count, level count, and root keys.

### B+ tree rebalancing

All keys greater than the threshold form the rightmost portion of the sorted
index. Deleting from largest to smallest therefore always follows the tree's
rightmost path; only left siblings are needed for repair.

- If a non-root leaf has fewer than `floor((n + 1) / 2)` entries, borrow the
  largest entry from its left sibling when that sibling can spare one. Update
  the parent separator accordingly.
- Otherwise, merge the leaf into its left sibling, repair the leaf link, and
  remove the corresponding parent separator and child pointer.
- If an internal node has fewer than `floor(n / 2) + 1` children, borrow a child
  through the parent separator or merge with the left sibling. Continue repairs
  towards the root as needed.
- If the root has only one child, that child becomes the new root and the tree
  height decreases. An empty index is represented by one empty root leaf.

The added API is:

```cpp
void BPlusTree::eraseLargest(float expectedKey, const RecordId& expectedRid);
```

This method is specialized for removing the current greatest entry. It supports
this suffix-deletion query, but it is not a general arbitrary-key deletion API.

### Task 3 results

The following reference results were obtained using the supplied full dataset
in its original, unsorted storage order and a threshold of `0.5`.

| Statistic | Result |
| --- | ---: |
| Original live games | 26,651 |
| Games deleted | 6,054 |
| Average `FG_PCT_home` of retrieved/deleted records | 0.536965643 |
| Remaining live games, including NULL keys | 20,597 |
| Remaining indexed entries | 20,498 |
| Index node read accesses during retrieval | 17 |
| Data block read accesses during indexed retrieval | 151 |
| Data block read accesses during linear retrieval | 151 |
| Updated reachable tree nodes | 52 |
| Updated leaf nodes | 51 |
| Updated internal nodes | 1 |
| Updated levels | 2 |
| Updated root keys | 50 |

The updated root keys are:

```text
0.342 0.358 0.368 0.375 0.381 0.386 0.391 0.395 0.400 0.402
0.406 0.410 0.413 0.416 0.418 0.420 0.424 0.427 0.429 0.431
0.433 0.436 0.438 0.440 0.443 0.446 0.447 0.449 0.452 0.455
0.457 0.458 0.461 0.463 0.465 0.468 0.470 0.471 0.474 0.476
0.479 0.481 0.483 0.487 0.488 0.493 0.494 0.495 0.500 0.500
```

The 848 games with `FG_PCT_home = 0.5` remain. The 99 records with missing keys
also remain in storage, explaining the difference between live records and
indexed entries.

Retrieval and deletion times are printed on each run. Use your own machine's
measured times in the report; the reference implementation was tested on
Linux/GCC, and those timings do not represent a Windows run.

### Access counts and timing definitions

The report separates the retrieval comparison from deletion and also provides
combined costs for the indexed process.

| Measurement | Meaning |
| --- | --- |
| Indexed retrieval | Tree descent, leaf scan, grouping addresses, reading matching records, and calculating their average |
| Linear retrieval | Checking every live record in every data block, collecting matches, and calculating their average |
| Deletion only | Removing/rebalancing index entries and tombstone-deleting stored records |
| Whole indexed process | Indexed retrieval plus deletion, excluding the comparison scan |

Counts represent logical 4096-byte block I/O calls, not physical device reads.
Repeated accesses count again. Reads and writes are reported separately, with
combined totals also provided. Header writes are listed separately because
headers are neither index nodes nor data blocks.

For this dataset, the full indexed process makes **302 data-block reads**:
151 during retrieval and 151 during deletion. It also makes 151 data-block writes.
The number of **distinct affected data blocks is 151**.

File copying, opening headers, validation, final statistics, and report writing
are excluded from the measured phases. Both retrieval methods run before
deletion; indexed retrieval runs first. OS caches are not cleared, so times are
single-run elapsed measurements rather than controlled cold-cache benchmarks.

**Why both methods read 151 data blocks:** the matching games are scattered
across every data block in the unsorted file. The indexed query therefore reads
all data blocks as well as the index nodes and performs address-grouping work.
For this query, a linear scan can be faster. Report the measured result rather
than assuming an index must improve performance.

### Output files and validation

Each run creates these files under `task3_runs/run_NNN`, relative to the working
directory:

| File | Content |
| --- | --- |
| `results.txt` | Statistics, timings, updated root keys, and validation results |
| `deleted_records.csv` | Matching key values and original block/slot addresses; not a complete export of every game field |
| `data_after.db` | Database copy after deletion |
| `index_after.db` | B+ tree copy after deletion |

A successful run prints:

```text
Indexed results equal linear scan: PASS
Reopened database and B+ tree validation: PASS
```

Validation checks sorted keys, occupancy limits, parent ranges, equal leaf
depths, leaf links, header counts, and agreement with live stored records. The
program also checks per-block live-record counts and the database record count.

Deleted data records remain as tombstones. Merged-away index nodes remain
allocated in the file but are unlinked and excluded from the reachable node
count. File sizes therefore do not shrink; this implementation does not compact
files or reuse retired index blocks. Deletion is not transactional: an
interrupted run may leave an incomplete working copy, while the original inputs
remain available for a new run.
