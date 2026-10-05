// task1.cpp - Task 1 driver: store games.txt in the simulated disk and report
// the storage statistics.
//
// Usage: task1 [games.txt] [data.db] [--sorted] [--show-block K]
//   --sorted        store records in FG_PCT_home order (sequential file)
//   --show-block K  print the header and first records of data block K
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "storage.h"

using namespace storage;

namespace {

// What the record would cost if we simply wrote a C++ struct to disk:
// the compiler pads fields to their alignment (Tutorial 1 Q2).
struct NaiveRecord {
    std::uint8_t header;
    std::uint16_t gameDate;
    std::uint32_t teamIdHome;
    std::uint8_t ptsHome;
    float fgPctHome, ftPctHome, fg3PctHome;
    std::uint8_t astHome, rebHome, homeTeamWins;
};

void row(const char* label, const std::string& value) { std::printf("  %-40s %s\n", label, value.c_str()); }

void showBlock(StorageManager& db, BlockId id) {
    DataBlock b = db.readDataBlock(id);
    std::printf("\nData block %u: header {block id=%u, slots used=%u, live=%u}\n", id, id, b.slotsUsed(),
                b.liveCount());
    for (std::uint16_t s = 0; s < b.slotsUsed() && s < 5; ++s)
        std::printf("  slot %3u  %s\n", s, b.record(s).toString().c_str());
    if (b.slotsUsed() > 5) std::printf("  ... %u more slots\n", b.slotsUsed() - 5);
    std::printf("  bytes of slot 0 (%zu B):", RECORD_SIZE);
    for (std::size_t i = 0; i < RECORD_SIZE; ++i) std::printf(" %02x", b.bytes()[BLOCK_HEADER_SIZE + i]);
    std::printf("\n");
}

}  // namespace

int main(int argc, char** argv) {
    std::string tsv = "games.txt", dbPath = "data.db";
    bool sorted = false;
    long showBlockId = -1;
    int positional = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--sorted") == 0) sorted = true;
        else if (std::strcmp(argv[i], "--show-block") == 0 && i + 1 < argc) showBlockId = std::atol(argv[++i]);
        else if (positional == 0) { tsv = argv[i]; ++positional; }
        else { dbPath = argv[i]; ++positional; }
    }

    try {
        // ---- Build the database file ----
        StorageManager db(dbPath, /*createNew=*/true);
        std::uint32_t loaded = db.loadFromTsv(tsv, sorted);
        StorageStats s = db.stats();

        std::printf("=== Task 1: storage component ===\n");
        std::printf("Loaded %u records from %s into %s\n\n", loaded, tsv.c_str(), dbPath.c_str());

        std::printf("Design\n");
        row("Record", std::to_string(s.recordSize) + " B fixed length: 1 B header + 22 B of fields, no padding");
        row("Block", std::to_string(s.blockSize) + " B = " + std::to_string(s.blockHeaderSize) + " B header + " +
                         std::to_string(s.recordsPerBlock) + " x " + std::to_string(s.recordSize) + " B slots + " +
                         std::to_string(s.unusedBytesPerBlock) + " B unused (unspanned)");
        row("Database file", "block 0 = file header, blocks 1.." + std::to_string(s.numDataBlocks) +
                                 " = data blocks (contiguous), " + std::to_string(s.fileBytes) + " B total");
        row("Record order", s.sortedByKey ? "sorted by FG_PCT_home" : "same order as games.txt (heap file)");

        std::printf("\nStatistics\n");
        row("Size of a record", std::to_string(s.recordSize) + " bytes");
        row("Number of records", std::to_string(s.numRecords));
        row("Number of records stored in a block", std::to_string(s.recordsPerBlock));
        row("Number of blocks for storing the data", std::to_string(s.numDataBlocks));
        row("Records with missing values", std::to_string(s.recordsWithNulls));
        std::printf("  (a plain C++ struct would be %zu B -> %zu records/block)\n", sizeof(NaiveRecord),
                    (BLOCK_SIZE - BLOCK_HEADER_SIZE) / sizeof(NaiveRecord));

        // ---- Verify: reopen the file and compare every stored record with games.txt ----
        std::vector<Record> expected;
        {
            std::ifstream in(tsv);
            std::string line;
            std::size_t n = 0;
            while (std::getline(in, line))
                if (++n > 1 && !line.empty() && line != "\r") expected.push_back(parseLine(line, n));
        }
        StorageManager reopened(dbPath, /*createNew=*/false);
        std::size_t idx = 0, matches = 0;
        for (BlockId id = reopened.firstDataBlock(); id < reopened.endDataBlock(); ++id) {
            DataBlock b = reopened.readDataBlock(id);
            for (std::uint16_t slot = 0; slot < b.slotsUsed(); ++slot, ++idx) {
                std::uint8_t a[RECORD_SIZE], e[RECORD_SIZE];
                serialize(b.record(slot), a);
                if (idx < expected.size()) serialize(expected[idx], e);
                if (!sorted && idx < expected.size() && std::memcmp(a, e, RECORD_SIZE) == 0) ++matches;
            }
        }
        std::printf("\nVerification: reopened %s, read %llu data blocks, %zu records\n", dbPath.c_str(),
                    static_cast<unsigned long long>(reopened.dataBlockReads()), idx);
        if (!sorted)
            std::printf("  %zu / %zu records identical to games.txt -> %s\n", matches, expected.size(),
                        matches == expected.size() && idx == expected.size() ? "OK" : "MISMATCH");

        if (showBlockId >= 0) showBlock(reopened, static_cast<BlockId>(showBlockId));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
    return 0;
}
