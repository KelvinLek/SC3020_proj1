#include "storage.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace storage {

namespace {

const char MAGIC[8] = {'S', 'C', '3', '0', '2', '0', 'D', 'B'};
const std::uint32_t VERSION = 1;
// Stored once in the file header instead of in every record (the record header
// would otherwise need a "pointer to schema", Lecture 03).
const char SCHEMA[] =
    "GAME_DATE_EST:DATE16|TEAM_ID_home:U32|PTS_home:U8|FG_PCT_home:F32|FT_PCT_home:F32|"
    "FG3_PCT_home:F32|AST_home:U8|REB_home:U8|HOME_TEAM_WINS:U8";

std::uint16_t get16(const std::uint8_t* p) { std::uint16_t v; std::memcpy(&v, p, 2); return v; }
std::uint32_t get32(const std::uint8_t* p) { std::uint32_t v; std::memcpy(&v, p, 4); return v; }
void put16(std::uint8_t* p, std::uint16_t v) { std::memcpy(p, &v, 2); }
void put32(std::uint8_t* p, std::uint32_t v) { std::memcpy(p, &v, 4); }

std::size_t slotOffset(std::uint16_t slot) { return BLOCK_HEADER_SIZE + std::size_t(slot) * RECORD_SIZE; }

}  // namespace

// ---------------- DataBlock ----------------

std::uint16_t DataBlock::slotsUsed() const { return get16(bytes_.data() + 4); }
std::uint16_t DataBlock::liveCount() const { return get16(bytes_.data() + 6); }

bool DataBlock::isLive(std::uint16_t slot) const {
    return slot < slotsUsed() && !isDeleted(bytes_.data() + slotOffset(slot));
}

Record DataBlock::record(std::uint16_t slot) const {
    if (slot >= slotsUsed()) throw std::out_of_range("DataBlock: empty slot");
    return deserialize(bytes_.data() + slotOffset(slot));
}

// ---------------- StorageManager ----------------

StorageManager::StorageManager(const std::string& dbPath, bool createNew)
    : disk_(new Disk(dbPath, createNew)) {
    if (createNew) {
        disk_->allocateBlock();   // block 0 = file header
        writeHeader();
    } else {
        readHeader();
    }
}

void StorageManager::writeHeader() {
    std::vector<std::uint8_t> b(BLOCK_SIZE, 0);
    std::memcpy(b.data(), MAGIC, 8);
    put32(&b[8], VERSION);
    put32(&b[12], static_cast<std::uint32_t>(BLOCK_SIZE));
    put32(&b[16], static_cast<std::uint32_t>(RECORD_SIZE));
    put32(&b[20], static_cast<std::uint32_t>(BLOCK_HEADER_SIZE));
    put32(&b[24], static_cast<std::uint32_t>(RECORDS_PER_BLOCK));
    put32(&b[28], numDataBlocks_);
    put32(&b[32], numRecords_);
    put32(&b[36], recordsWithNulls_);
    b[40] = sortedByKey_ ? 1 : 0;
    std::memcpy(&b[48], SCHEMA, sizeof SCHEMA);
    disk_->writeBlock(HEADER_BLOCK_ID, b.data());
}

void StorageManager::readHeader() {
    std::vector<std::uint8_t> b(BLOCK_SIZE);
    disk_->readBlock(HEADER_BLOCK_ID, b.data());
    if (std::memcmp(b.data(), MAGIC, 8) != 0) throw std::runtime_error("not an SC3020 database file");
    if (get32(&b[12]) != BLOCK_SIZE || get32(&b[16]) != RECORD_SIZE)
        throw std::runtime_error("database file was built with a different BLOCK_SIZE/RECORD_SIZE");
    numDataBlocks_ = get32(&b[28]);
    numRecords_ = get32(&b[32]);
    recordsWithNulls_ = get32(&b[36]);
    sortedByKey_ = b[40] != 0;
}

std::uint32_t StorageManager::loadFromTsv(const std::string& tsvPath, bool sortByKey) {
    std::ifstream in(tsvPath);
    if (!in) throw std::runtime_error("cannot open " + tsvPath);

    // 1. Parse every data line (line 1 is the column header).
    std::vector<Record> records;
    std::string line;
    std::size_t lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (lineNo == 1) continue;
        if (line.empty() || line == "\r") continue;
        records.push_back(parseLine(line, lineNo));
    }

    // 2. Optional: store as a sequential file on FG_PCT_home (NULL keys last).
    if (sortByKey) {
        std::stable_sort(records.begin(), records.end(), [](const Record& a, const Record& b) {
            bool an = a.isNull(NULL_FG_PCT), bn = b.isNull(NULL_FG_PCT);
            if (an != bn) return bn;   // non-null before null
            return !an && a.fgPctHome < b.fgPctHome;
        });
    }

    // 3. Pack RECORDS_PER_BLOCK records into each block and write the blocks in order.
    std::vector<std::uint8_t> block(BLOCK_SIZE);
    for (std::size_t i = 0; i < records.size(); i += RECORDS_PER_BLOCK) {
        std::fill(block.begin(), block.end(), 0);
        auto n = static_cast<std::uint16_t>(std::min(RECORDS_PER_BLOCK, records.size() - i));
        BlockId id = disk_->allocateBlock();
        put32(&block[0], id);
        put16(&block[4], n);
        put16(&block[6], n);
        for (std::uint16_t s = 0; s < n; ++s) {
            serialize(records[i + s], &block[slotOffset(s)]);
            if (records[i + s].nullMask) ++recordsWithNulls_;
        }
        disk_->writeBlock(id, block.data());
        ++numDataBlocks_;
    }
    numRecords_ += static_cast<std::uint32_t>(records.size());
    sortedByKey_ = sortByKey;
    writeHeader();
    disk_->flush();
    return static_cast<std::uint32_t>(records.size());
}

DataBlock StorageManager::readDataBlock(BlockId id) {
    if (id < firstDataBlock() || id >= endDataBlock()) throw std::out_of_range("not a data block id");
    std::vector<std::uint8_t> bytes(BLOCK_SIZE);
    disk_->readBlock(id, bytes.data());
    ++dataBlockReads_;
    return DataBlock(id, std::move(bytes));
}

Record StorageManager::getRecord(const RecordId& rid) {
    DataBlock b = readDataBlock(rid.block);
    if (!b.isLive(rid.slot)) throw std::runtime_error("record is deleted or slot is empty");
    return b.record(rid.slot);
}

bool StorageManager::deleteRecord(const RecordId& rid) {
    return deleteRecords({rid}) == 1;
}

std::uint32_t StorageManager::deleteRecords(std::vector<RecordId> rids) {
    std::sort(rids.begin(), rids.end(), [](const RecordId& a, const RecordId& b) {
        return a.block != b.block ? a.block < b.block : a.slot < b.slot;
    });
    std::uint32_t deleted = 0;
    std::size_t i = 0;
    while (i < rids.size()) {
        BlockId id = rids[i].block;
        DataBlock b = readDataBlock(id);                 // 1 read per affected block
        std::vector<std::uint8_t> bytes = b.bytes();
        std::uint16_t live = b.liveCount();
        for (; i < rids.size() && rids[i].block == id; ++i) {
            std::uint16_t s = rids[i].slot;
            if (!b.isLive(s)) continue;
            bytes[slotOffset(s)] |= DELETED_BIT;          // tombstone: no shifting
            --live;
            ++deleted;
        }
        put16(&bytes[6], live);
        disk_->writeBlock(id, bytes.data());             // 1 write per affected block
    }
    numRecords_ -= deleted;
    writeHeader();
    return deleted;
}

RecordId StorageManager::insertRecord(const Record& r) {
    std::vector<std::uint8_t> bytes(BLOCK_SIZE, 0);
    BlockId id = 0;
    std::uint16_t slot = 0;
    if (numDataBlocks_ > 0) {
        id = endDataBlock() - 1;
        disk_->readBlock(id, bytes.data());
        slot = get16(&bytes[4]);
    } else {
        slot = RECORDS_PER_BLOCK;   // forces a new block
    }
    if (slot >= RECORDS_PER_BLOCK) {   // last block full: append a new one
        std::fill(bytes.begin(), bytes.end(), 0);
        id = disk_->allocateBlock();
        put32(&bytes[0], id);
        slot = 0;
        ++numDataBlocks_;
    }
    serialize(r, &bytes[slotOffset(slot)]);
    put16(&bytes[4], static_cast<std::uint16_t>(slot + 1));
    put16(&bytes[6], static_cast<std::uint16_t>(get16(&bytes[6]) + 1));
    disk_->writeBlock(id, bytes.data());
    ++numRecords_;
    if (r.nullMask) ++recordsWithNulls_;
    sortedByKey_ = false;   // an appended record breaks the key order
    writeHeader();
    return {id, slot};
}

StorageStats StorageManager::stats() const {
    StorageStats s;
    s.numRecords = numRecords_;
    s.numDataBlocks = numDataBlocks_;
    s.recordsWithNulls = recordsWithNulls_;
    s.fileBytes = std::uint64_t(disk_->numBlocks()) * BLOCK_SIZE;
    s.sortedByKey = sortedByKey_;
    return s;
}

}  // namespace storage
