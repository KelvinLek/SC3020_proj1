# Storage component (Task 1) - Member 1

Stores `games.txt` in a simulated disk (`data.db`, a binary file split into 4 KB blocks) and reports the Task 1 statistics. The B+ tree (Tasks 2-3) builds on the API below.

## Install and run

Needs a C++17 compiler, and nothing else.

| OS | Compiler |
|---|---|
| Windows | MSYS2/MinGW-w64 `g++` (or MSVC: `cl /std:c++17 /EHsc src\task1.cpp src\disk.cpp src\record.cpp src\storage.cpp`) |
| macOS | `xcode-select --install`, then `g++` (clang) |
| Linux | `sudo apt install g++` |

```bash
g++ -std=c++17 -O2 -o task1 src/task1.cpp src/disk.cpp src/record.cpp src/storage.cpp
./task1 games.txt data.db --show-block 1      # Windows: task1.exe games.txt data.db --show-block 1
```

Or just `make run`. Options: `--sorted` stores records in FG_PCT_home order, and `--show-block K` prints data block K (useful for the video demo).

## Files

| File | What it does |
|---|---|
| `src/config.h` | `BLOCK_SIZE` (4096) for the whole project |
| `src/disk.h/.cpp` | `Disk`: block-level read/write on one binary file, with I/O counters |
| `src/record.h/.cpp` | `Record` (23 B layout), `RecordId`, parsing, (de)serialisation |
| `src/storage.h/.cpp` | `StorageManager`: file header, loading, block/record access, delete, insert |
| `src/task1.cpp` | Task 1 driver: build `data.db`, print statistics, verify every record |

## API for Members 2-5

```cpp
#include "storage.h"
using namespace storage;

StorageManager db("data.db", /*createNew=*/false);          // open what task1 built

// Member 3 - build the tree: one pass over the data blocks
for (BlockId b = db.firstDataBlock(); b < db.endDataBlock(); ++b) {
    DataBlock blk = db.readDataBlock(b);                    // 1 data-block read
    for (uint16_t s = 0; s < blk.slotsUsed(); ++s)
        if (blk.isLive(s) && !blk.record(s).isNull(NULL_FG_PCT)) {
            float key = blk.record(s).fgPctHome;
            RecordId ptr = blk.recordId(s);                  // tree.insert(key, ptr);
        }
}

// Member 2 - store record pointers inside leaf nodes (6 bytes each)
uint8_t buf[RECORD_ID_SIZE];  encodeRecordId(rid, buf);  RecordId r = decodeRecordId(buf);
// A disk-based tree can use its own simulated disk: Disk idx("index.db", true);

// Member 4 - fetch and delete
db.resetDataBlockReads();
Record rec = db.getRecord(rid);                             // 1 data-block read
db.deleteRecords(rids);                                     // each affected block read + written once
uint64_t blocksAccessed = db.dataBlockReads();

// Member 5 - linear scan: loop readDataBlock over [firstDataBlock(), endDataBlock())
```

## Points the team should agree on

- **NULL keys:** 99 records have an empty FG_PCT_home (and empty PTS/FT/FG3/AST/REB). They are stored with null flags. The tree should skip them (`isNull(NULL_FG_PCT)`).
- **Key type:** FG_PCT_home is a 4-byte `float`. 848 records are exactly 0.500, which is representable exactly, so `> 0.5f` excludes them correctly in Task 3.
- **Deletion:** a tombstone bit, not shifting. RecordIds never move, so the tree's pointers stay valid. `deleteRecords` batches by block.
- **Record order:** the default is the order of games.txt, so a B+ tree on FG_PCT_home is a dense, **non-clustered** index. `--sorted` makes the file sequential on the key, which gives a clustered index. The games with FG_PCT_home > 0.5 sit in all 151 blocks in file order, but in only 36 blocks when sorted. Decide this together because it changes Task 3's numbers.
- **Counting:** `db.dataBlockReads()` counts data blocks only. Index-node reads should be counted on the tree's own `Disk`.
