// task2.cpp - builds the B+ tree on FG_PCT_home and prints the Task 2 statistics.
// Usage: task2 [data.db] [--index index.db] [--limit K] [--print]
#include <iomanip>
#include <iostream>
#include <string>

#include "storage.h"
#include "tree_build.h"

using namespace storage;
using namespace bptree;

int main(int argc, char** argv) {
    std::string dbPath = "../Storage/data.db";
    std::string indexPath = "index.db";
    std::size_t limit = 0;
    bool print = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--index" && i + 1 < argc) indexPath = argv[++i];
        else if (a == "--limit" && i + 1 < argc) limit = std::stoul(argv[++i]);
        else if (a == "--print") print = true;
        else dbPath = a;
    }

    try {
        StorageManager db(dbPath, /*createNew=*/false);
        std::vector<Entry> entries;
        std::size_t skippedNull = 0;
        for (BlockId b = db.firstDataBlock(); b < db.endDataBlock(); ++b) {
            DataBlock blk = db.readDataBlock(b);
            for (std::uint16_t s = 0; s < blk.slotsUsed(); ++s) {
                if (!blk.isLive(s)) continue;
                Record r = blk.record(s);
                if (r.isNull(NULL_FG_PCT)) { ++skippedNull; continue; }
                if (limit && entries.size() >= limit) break;
                entries.push_back({r.fgPctHome, blk.recordId(s)});
            }
        }

        std::uint64_t blockWrites = 0;
        {
            BPlusTree tree(indexPath, /*createNew=*/true);
            tree.disk().resetCounters();
            bulkLoad(tree, entries);
            blockWrites = tree.disk().writes();
        }

        // reopen so the stats come from what is actually on disk
        BPlusTree tree(indexPath, /*createNew=*/false);
        tree.disk().resetCounters();
        TreeStats s = treeStats(tree);

        std::cout << "=== Task 2: B+ tree on FG_PCT_home ===\n"
                  << "Indexed " << entries.size() << " records (" << skippedNull
                  << " with empty FG_PCT_home skipped), " << db.dataBlockReads() << " data blocks read\n"
                  << "Built by bulk loading into " << indexPath << ": " << tree.disk().numBlocks()
                  << " blocks (1 header + " << tree.numNodes() << " nodes), "
                  << blockWrites << " block writes\n"
                  << "Statistics read back from " << indexPath << ": " << tree.disk().reads()
                  << " node reads\n\n"
                  << "Parameter n                     " << s.n << "\n"
                  << "Number of nodes                 " << s.numNodes
                  << " (" << s.numInternal << " internal + " << s.numLeaves << " leaves)\n"
                  << "Number of levels                " << s.levels << "\n"
                  << "Root node keys (" << s.rootKeys.size() << ")            ";
        std::cout << std::fixed << std::setprecision(3);
        for (std::size_t i = 0; i < s.rootKeys.size(); ++i) std::cout << (i ? " " : "") << s.rootKeys[i];
        std::cout << "\n";
        std::cout.unsetf(std::ios::floatfield);

        if (print) std::cout << "\n" << dumpTree(tree);

        std::string problem = validate(tree, entries);
        std::cout << "\nValidation: " << (problem.empty() ? "OK" : "FAILED - " + problem) << "\n";
        return problem.empty() ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
