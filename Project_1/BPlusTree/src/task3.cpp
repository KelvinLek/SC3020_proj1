// Task 3 driver. Run from Project_1 after Tasks 1 and 2.
// Usage: task3 [data.db] [index.db] [threshold]
// Defaults: Storage/data.db BPlusTree/index.db 0.5
// Creates a fresh task3_runs/run_NNN folder; originals are never modified.
#include "storage.h"
#include "tree_build.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace storage;
using namespace bptree;
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace {
struct Result {
    std::vector<Entry> entries;
    double sum = 0;
    std::uint64_t indexReads = 0, dataReads = 0;
    double milliseconds = 0;
};

double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

Result indexedRetrieval(BPlusTree& tree, StorageManager& db, float threshold) {
    Result result;
    tree.disk().resetCounters(); // Opening the index read its header; exclude it.
    db.resetDataBlockReads();
    const auto start = Clock::now();
    if (tree.numLevels() != 0 && tree.header().numKeys != 0) {
        BlockId id = tree.rootNode();
        for (std::uint32_t depth = 1; depth < tree.numLevels(); ++depth) {
            InternalNode node = tree.readInternal(id);
            // Strict > query: skip separators equal to the threshold.
            // Their left subtrees cannot contain a key greater than threshold.
            auto child = std::upper_bound(node.keys, node.keys + node.numKeys, threshold)
                       - node.keys;
            id = node.children[child];
        }
        while (id != NO_NEXT_LEAF) {
            LeafNode leaf = tree.readLeaf(id);
            for (std::size_t j = 0; j < leaf.numKeys; ++j)
                if (leaf.keys[j] > threshold)
                    result.entries.push_back({leaf.keys[j], leaf.recordIds[j]});
            id = leaf.nextLeaf;
        }
    }

    // Group the matching addresses by block. Each data block is read once.
    // Preserve entries in tree order for later deletion from largest downward.
    std::map<BlockId, std::vector<std::size_t>> groups;
    for (std::size_t i = 0; i < result.entries.size(); ++i)
        groups[result.entries[i].rid.block].push_back(i);
    for (const auto& group : groups) {
        DataBlock block = db.readDataBlock(group.first);
        for (std::size_t i : group.second) {
            const Entry& e = result.entries[i];
            if (!block.isLive(e.rid.slot))
                throw std::runtime_error("Index points to a deleted/unused record");
            Record record = block.record(e.rid.slot);
            if (record.isNull(NULL_FG_PCT) || !(record.fgPctHome > threshold) ||
                record.fgPctHome != e.key)
                throw std::runtime_error("Index key does not match stored record");
            result.sum += record.fgPctHome;
        }
    }
    result.milliseconds = elapsed(start);
    result.indexReads = tree.disk().reads();
    result.dataReads = db.dataBlockReads();
    return result;
}

Result linearRetrieval(StorageManager& db, float threshold) {
    Result result;
    db.resetDataBlockReads();
    const auto start = Clock::now();
    for (BlockId id = db.firstDataBlock(); id < db.endDataBlock(); ++id) {
        DataBlock block = db.readDataBlock(id);
        for (std::uint16_t slot = 0; slot < block.slotsUsed(); ++slot) {
            if (!block.isLive(slot)) continue;
            Record record = block.record(slot);
            if (!record.isNull(NULL_FG_PCT) && record.fgPctHome > threshold) {
                result.entries.push_back({record.fgPctHome, block.recordId(slot)});
                result.sum += record.fgPctHome;
            }
        }
    }
    result.milliseconds = elapsed(start);
    result.dataReads = db.dataBlockReads();
    return result;
}

std::vector<Entry> liveEntries(StorageManager& db) {
    std::vector<Entry> entries;
    std::size_t live = 0;
    for (BlockId id = db.firstDataBlock(); id < db.endDataBlock(); ++id) {
        DataBlock block = db.readDataBlock(id);
        std::size_t inBlock = 0;
        for (std::uint16_t slot = 0; slot < block.slotsUsed(); ++slot) {
            if (!block.isLive(slot)) continue;
            ++live;
            ++inBlock;
            Record r = block.record(slot);
            if (!r.isNull(NULL_FG_PCT)) entries.push_back({r.fgPctHome, block.recordId(slot)});
        }
        if (inBlock != block.liveCount()) throw std::runtime_error("Wrong block live count");
    }
    if (live != db.numRecords()) throw std::runtime_error("Wrong database live count");
    return entries;
}

bool sameEntries(std::vector<Entry> a, std::vector<Entry> b) {
    std::sort(a.begin(), a.end(), entryLess);
    std::sort(b.begin(), b.end(), entryLess);
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (entryLess(a[i], b[i]) || entryLess(b[i], a[i])) return false;
    return true;
}

fs::path newRunDirectory() {
    fs::create_directories("task3_runs");
    for (unsigned i = 1; i < 1000000; ++i) {
        std::ostringstream name;
        name << "run_" << std::setw(3) << std::setfill('0') << i;
        fs::path path = fs::path("task3_runs") / name.str();
        if (fs::create_directory(path)) return path;
    }
    throw std::runtime_error("Too many run folders");
}

void writeText(const fs::path& path, const std::string& text) {
    std::ofstream out(path);
    out << text;
    out.close();
    if (!out) throw std::runtime_error("Cannot write " + path.string());
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 4) throw std::runtime_error("Usage: task3 [data.db] [index.db] [threshold]");
        const std::string sourceData = argc > 1 ? argv[1] : "Storage/data.db";
        const std::string sourceIndex = argc > 2 ? argv[2] : "BPlusTree/index.db";
        float threshold = 0.5f;
        if (argc > 3) {
            std::size_t consumed = 0;
            threshold = std::stof(argv[3], &consumed);
            if (consumed != std::string(argv[3]).size() || !std::isfinite(threshold))
                throw std::runtime_error("Threshold must be a finite number");
        }
        if (!fs::is_regular_file(sourceData) || !fs::is_regular_file(sourceIndex))
            throw std::runtime_error("Missing input database/index. Run Tasks 1 and 2 first.");
        const fs::path folder = newRunDirectory();
        const fs::path dataPath = folder / "data_after.db";
        const fs::path indexPath = folder / "index_after.db";
        fs::copy_file(sourceData, dataPath);
        fs::copy_file(sourceIndex, indexPath);
        std::cout << "Working copies: " << folder.string() << "\n";

        Result indexed, linear;
        TreeStats before, after;
        std::uint32_t originalRecords = 0, remainingRecords = 0, deleted = 0;
        std::uint64_t deleteIndexReads = 0, deleteIndexWrites = 0;
        std::uint64_t deleteDataReads = 0, deleteDataWrites = 0;
        double deletionMs = 0;
        {
            StorageManager db(dataPath.string(), false);
            BPlusTree tree(indexPath.string(), false);
            originalRecords = db.numRecords();

            // Both retrievals see the same original records: no deletion yet.
            indexed = indexedRetrieval(tree, db, threshold);
            linear = linearRetrieval(db, threshold);
            if (!sameEntries(indexed.entries, linear.entries) ||
                std::abs(indexed.sum - linear.sum) > 1e-7)
                throw std::runtime_error("Indexed retrieval differs from linear scan; no deletion done");

            // Validate the COMPLETE index against storage before mutation.
            // This also rejects indexes built with Task 2's --limit option.
            const std::string problem = validate(tree, liveEntries(db));
            if (!problem.empty()) throw std::runtime_error("Pre-deletion validation: " + problem);
            before = treeStats(tree);

            std::vector<RecordId> rids;
            for (const Entry& e : indexed.entries) rids.push_back(e.rid);
            tree.disk().resetCounters();
            db.disk().resetCounters();
            db.resetDataBlockReads();
            const auto deletionStart = Clock::now();
            for (auto it = indexed.entries.rbegin(); it != indexed.entries.rend(); ++it)
                tree.eraseLargest(it->key, it->rid);
            if (!rids.empty()) deleted = db.deleteRecords(rids);
            if (deleted != rids.size()) throw std::runtime_error("Unexpected deletion count");
            tree.disk().flush();
            db.disk().flush();
            deletionMs = elapsed(deletionStart);
            deleteIndexReads = tree.disk().reads();
            deleteIndexWrites = tree.disk().writes();
            deleteDataReads = db.dataBlockReads();
            deleteDataWrites = db.disk().writes();
            remainingRecords = db.numRecords();
        }
        // Close and reopen: verify persisted files, outside measured phases.
        {
            StorageManager db(dataPath.string(), false);
            BPlusTree tree(indexPath.string(), false);
            auto remaining = liveEntries(db);
            for (const Entry& e : remaining)
                if (e.key > threshold) throw std::runtime_error("A matching record survived deletion");
            const std::string problem = validate(tree, remaining);
            if (!problem.empty()) throw std::runtime_error("Post-deletion validation: " + problem);
            if (remainingRecords != originalRecords - deleted)
                throw std::runtime_error("Unexpected remaining record count");
            after = treeStats(tree);
        }

        // Tables are directly usable when preparing the experimental report.
        std::ostringstream report;
        report << std::fixed << std::setprecision(6);
        report << "=== Task 3: delete FG_PCT_home > " << threshold << " ===\n\n";
        report << "Metric | B+ tree retrieval | Linear scan retrieval\n";
        report << "Index node read accesses | " << indexed.indexReads << " | 0\n";
        report << "Data block read accesses | " << indexed.dataReads << " | " << linear.dataReads << "\n";
        report << "Matching games | " << indexed.entries.size() << " | " << linear.entries.size() << "\n";
        report << "Average FG_PCT_home | ";
        if (indexed.entries.empty()) report << "N/A | N/A\n";
        else report << std::setprecision(9) << indexed.sum / indexed.entries.size() << " | "
                    << linear.sum / linear.entries.size() << "\n";
        report << std::setprecision(6);
        report << "Retrieval time (ms) | " << indexed.milliseconds << " | " << linear.milliseconds << "\n\n";
        // One header write per eraseLargest; one database header write for a nonempty batch.
        const auto indexNodeWrites = deleteIndexWrites - deleted;
        const auto dataBlockWrites = deleteDataWrites - (deleted ? 1u : 0u);
        report << "Deletion / whole indexed process | Value\n";
        report << "Games deleted | " << deleted << "\n";
        report << "Remaining live games (including NULL keys) | " << remainingRecords << "\n";
        report << "Deletion-only time (ms) | " << deletionMs << "\n";
        report << "Retrieval + deletion time (ms) | " << indexed.milliseconds + deletionMs << "\n";
        report << "Deletion-only index node reads | " << deleteIndexReads << "\n";
        report << "Deletion-only index node writes | " << indexNodeWrites << "\n";
        report << "Deletion-only data block reads | " << deleteDataReads << "\n";
        report << "Deletion-only data block writes | " << dataBlockWrites << "\n";
        report << "Whole indexed process: index node reads | " << indexed.indexReads + deleteIndexReads << "\n";
        report << "Whole indexed process: index node reads + writes | "
               << indexed.indexReads + deleteIndexReads + indexNodeWrites << "\n";
        report << "Whole indexed process: data block reads | " << indexed.dataReads + deleteDataReads << "\n";
        report << "Whole indexed process: data block reads + writes | "
               << indexed.dataReads + deleteDataReads + dataBlockWrites << "\n";
        report << "Whole indexed process: distinct data blocks | " << indexed.dataReads << "\n";
        report << "Index header writes (reported separately) | " << deleted << "\n";
        report << "Data header writes (reported separately) | " << (deleted ? 1 : 0) << "\n\n";
        report << "Tree statistic | Before | After\n";
        report << "Reachable nodes | " << before.numNodes << " | " << after.numNodes << "\n";
        report << "Leaf nodes | " << before.numLeaves << " | " << after.numLeaves << "\n";
        report << "Internal nodes | " << before.numInternal << " | " << after.numInternal << "\n";
        report << "Levels | " << before.levels << " | " << after.levels << "\n";
        report << "Indexed entries | " << before.numEntries << " | " << after.numEntries << "\n";
        report << "Updated root keys (" << after.rootKeys.size() << "):" << std::setprecision(3);
        for (float key : after.rootKeys) report << " " << key;
        report << "\n\nIndexed results equal linear scan: PASS\n";
        report << "Reopened database and B+ tree validation: PASS\n";
        report << "Original input files preserved. Updated files: " << folder.string() << "\n\n";
        report << "Measurement notes:\n"
               << "- Access counts are logical 4096-byte block I/O calls, including repeated reads.\n"
               << "- During retrieval each visited index/data block is read once.\n"
               << "- Whole-process counts cover indexed retrieval + deletion, not the comparison scan.\n"
               << "- Opening headers, file copying, validation, statistics, and reporting are excluded.\n"
               << "- Both retrievals run before deletion. Index runs first; OS caches are not cleared.\n"
               << "- Timings are single-run elapsed times, not cold-disk benchmarks.\n"
               << "- Retired nodes remain allocated on disk but are unlinked and excluded from node counts.\n"
               << "- Data deletion uses tombstones, so physical data-block count/file size does not shrink.\n";
        writeText(folder / "results.txt", report.str());
        std::ostringstream csv;
        csv << "FG_PCT_home,block_id,slot\n" << std::fixed << std::setprecision(9);
        for (const Entry& e : indexed.entries) csv << e.key << ',' << e.rid.block << ',' << e.rid.slot << '\n';
        writeText(folder / "deleted_records.csv", csv.str());
        std::cout << report.str();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Task 3 error: " << e.what() << "\n";
        return 1;
    }
}
