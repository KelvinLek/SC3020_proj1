// disk.h - a simulated disk: one binary file split into fixed-size blocks.
//
// The block is the unit of transfer (Lecture 02): readBlock/writeBlock always
// move one whole block, and each call is counted as one I/O. Nothing above this
// class touches the file directly, so the I/O counters are always accurate.
#pragma once
#include <cstdint>
#include <fstream>
#include <string>

#include "config.h"

namespace storage {

using BlockId = std::uint32_t;

class Disk {
public:
    // createNew = true truncates/creates the file; false opens an existing one.
    Disk(const std::string& path, bool createNew, std::size_t blockSize = BLOCK_SIZE);

    Disk(const Disk&) = delete;
    Disk& operator=(const Disk&) = delete;

    std::size_t blockSize() const { return blockSize_; }
    BlockId numBlocks() const { return numBlocks_; }
    const std::string& path() const { return path_; }

    // Appends a zero-filled block at the end of the file and returns its id.
    BlockId allocateBlock();

    // Copies block `id` into `out` (must hold blockSize() bytes). 1 read I/O.
    void readBlock(BlockId id, std::uint8_t* out);

    // Overwrites block `id` with `data` (blockSize() bytes). 1 write I/O.
    void writeBlock(BlockId id, const std::uint8_t* data);

    void flush();

    // I/O counters, used by Tasks 2-3 to report block accesses.
    std::uint64_t reads() const { return reads_; }
    std::uint64_t writes() const { return writes_; }
    void resetCounters() { reads_ = writes_ = 0; }

private:
    std::fstream file_;
    std::string path_;
    std::size_t blockSize_;
    BlockId numBlocks_ = 0;
    std::uint64_t reads_ = 0;
    std::uint64_t writes_ = 0;
};

}  // namespace storage
