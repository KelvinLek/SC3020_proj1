// config.h - settings shared by the whole project (storage + B+ tree).
#pragma once
#include <cstddef>
#include <cstdint>

namespace storage {

// Size of one disk block in bytes. 4 KB is the typical block size (Lecture 02)
// and matches the OS page size. Every read/write moves exactly one block.
// Change it here only; the B+ tree should derive its n from the same value.
constexpr std::size_t BLOCK_SIZE = 4096;

// Block 0 of the database file is the file header; data blocks start at 1.
constexpr std::uint32_t HEADER_BLOCK_ID = 0;
constexpr std::uint32_t FIRST_DATA_BLOCK_ID = 1;

}  // namespace storage
