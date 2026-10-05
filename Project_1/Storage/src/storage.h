// storage.h - the storage component: records -> blocks -> one database file.
//
// Database file layout (the simulated disk):
//   block 0          file header: magic, sizes, counts, schema text
//   blocks 1..N      data blocks, physically contiguous (Lecture 03)
//
// Data block layout (BLOCK_SIZE bytes, unspanned, fixed-length slots):
//   [0..3]  block id (uint32)
//   [4..5]  slots used (uint16)   - slots ever filled in this block
//   [6..7]  live records (uint16) - slots used minus deleted
//   [8.. ]  slot 0, slot 1, ... each RECORD_SIZE bytes; the tail that cannot
//           hold a whole record is left unused (a record never spans blocks).
//
// Records are addressed physically by RecordId{block, slot}. Deletion marks the
// record's tombstone bit instead of shifting records, so RecordIds held by the
// B+ tree never change.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "config.h"
#include "disk.h"
#include "record.h"

namespace storage {

constexpr std::size_t BLOCK_HEADER_SIZE = 8;
constexpr std::size_t RECORDS_PER_BLOCK = (BLOCK_SIZE - BLOCK_HEADER_SIZE) / RECORD_SIZE;
static_assert(RECORDS_PER_BLOCK > 0, "BLOCK_SIZE too small for one record");

// Numbers reported for Task 1.
struct StorageStats {
    std::size_t recordSize = RECORD_SIZE;
    std::size_t blockSize = BLOCK_SIZE;
    std::size_t blockHeaderSize = BLOCK_HEADER_SIZE;
    std::size_t recordsPerBlock = RECORDS_PER_BLOCK;
    std::size_t unusedBytesPerBlock = BLOCK_SIZE - BLOCK_HEADER_SIZE - RECORDS_PER_BLOCK * RECORD_SIZE;
    std::uint32_t numRecords = 0;      // live records
    std::uint32_t numDataBlocks = 0;
    std::uint32_t recordsWithNulls = 0;
    std::uint64_t fileBytes = 0;       // including the header block
    bool sortedByKey = false;
};

// An in-memory copy of one data block, returned by StorageManager::readDataBlock.
class DataBlock {
public:
    DataBlock(BlockId id, std::vector<std::uint8_t> bytes) : id_(id), bytes_(std::move(bytes)) {}
    BlockId id() const { return id_; }
    std::uint16_t slotsUsed() const;
    std::uint16_t liveCount() const;
    bool isLive(std::uint16_t slot) const;          // false for deleted or unused slots
    Record record(std::uint16_t slot) const;        // decode the record in `slot`
    RecordId recordId(std::uint16_t slot) const { return {id_, slot}; }
    const std::vector<std::uint8_t>& bytes() const { return bytes_; }

private:
    BlockId id_;
    std::vector<std::uint8_t> bytes_;
};

class StorageManager {
public:
    // createNew = true builds an empty database file; false opens an existing one.
    StorageManager(const std::string& dbPath, bool createNew);

    // Task 1: reads games.txt, packs the records into blocks and writes them.
    // sortByKey = true stores records in FG_PCT_home order (sequential file,
    // which would make a B+ tree on FG_PCT_home a clustered index).
    // Returns the number of records loaded.
    std::uint32_t loadFromTsv(const std::string& tsvPath, bool sortByKey = false);

    // --- Access used by the B+ tree and Task 3 (each call = 1 data-block read) ---
    DataBlock readDataBlock(BlockId id);
    Record getRecord(const RecordId& rid);

    // Tombstone-deletes one record: 1 block read + 1 block write.
    bool deleteRecord(const RecordId& rid);
    // Deletes many records, touching each affected block once. Returns #deleted.
    std::uint32_t deleteRecords(std::vector<RecordId> rids);

    // Appends a record to the last data block (or a new block if it is full).
    RecordId insertRecord(const Record& r);

    // Data blocks are the ids [firstDataBlock(), endDataBlock()).
    BlockId firstDataBlock() const { return FIRST_DATA_BLOCK_ID; }
    BlockId endDataBlock() const { return FIRST_DATA_BLOCK_ID + numDataBlocks_; }
    std::uint32_t numDataBlocks() const { return numDataBlocks_; }
    std::uint32_t numRecords() const { return numRecords_; }

    // Data-block accesses since the last reset (header block excluded).
    std::uint64_t dataBlockReads() const { return dataBlockReads_; }
    void resetDataBlockReads() { dataBlockReads_ = 0; }

    StorageStats stats() const;
    Disk& disk() { return *disk_; }

private:
    void writeHeader();
    void readHeader();

    std::unique_ptr<Disk> disk_;
    std::uint32_t numDataBlocks_ = 0;
    std::uint32_t numRecords_ = 0;
    std::uint32_t recordsWithNulls_ = 0;
    bool sortedByKey_ = false;
    std::uint64_t dataBlockReads_ = 0;
};

}  // namespace storage
