#include "disk.h"

#include <stdexcept>
#include <vector>

namespace storage {

Disk::Disk(const std::string& path, bool createNew, std::size_t blockSize)
    : path_(path), blockSize_(blockSize) {
    auto mode = std::ios::in | std::ios::out | std::ios::binary;
    if (createNew) mode |= std::ios::trunc;
    file_.open(path, mode);
    if (!file_) throw std::runtime_error("Disk: cannot open " + path);

    file_.seekg(0, std::ios::end);
    std::streamoff bytes = file_.tellg();
    if (bytes % static_cast<std::streamoff>(blockSize_) != 0)
        throw std::runtime_error("Disk: file size is not a multiple of the block size");
    numBlocks_ = static_cast<BlockId>(bytes / static_cast<std::streamoff>(blockSize_));
}

BlockId Disk::allocateBlock() {
    std::vector<std::uint8_t> zeros(blockSize_, 0);
    BlockId id = numBlocks_++;
    writeBlock(id, zeros.data());
    return id;
}

void Disk::readBlock(BlockId id, std::uint8_t* out) {
    if (id >= numBlocks_) throw std::out_of_range("Disk: read past last block");
    file_.seekg(static_cast<std::streamoff>(id) * static_cast<std::streamoff>(blockSize_));
    file_.read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(blockSize_));
    if (!file_) throw std::runtime_error("Disk: read failed");
    ++reads_;
}

void Disk::writeBlock(BlockId id, const std::uint8_t* data) {
    if (id >= numBlocks_) throw std::out_of_range("Disk: write past last block");
    file_.seekp(static_cast<std::streamoff>(id) * static_cast<std::streamoff>(blockSize_));
    file_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(blockSize_));
    file_.flush();   // write-through: the block is on disk when writeBlock returns
    if (!file_) throw std::runtime_error("Disk: write failed");
    ++writes_;
}

void Disk::flush() { file_.flush(); }

}  // namespace storage
