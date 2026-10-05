#pragma once

// ============================================================
// Shared little-endian byte-packing helpers.
//
// These are used by every bplustree_*.cpp translation unit that
// needs to encode/decode fixed-width fields (u16, u32, float) to
// and from a raw 4096-byte node/header buffer. Pulled out into a
// header so the helpers aren't duplicated per file.
// ============================================================

#include <cstdint>
#include <cstring>

namespace bptree::detail {

inline void putU16(std::uint8_t* p, std::uint16_t value) {
    p[0] = static_cast<std::uint8_t>(value & 0xFF);
    p[1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

inline std::uint16_t getU16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(
        p[0] |
        (static_cast<std::uint16_t>(p[1]) << 8)
    );
}

inline void putU32(std::uint8_t* p, std::uint32_t value) {
    p[0] = static_cast<std::uint8_t>(value & 0xFF);
    p[1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    p[2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    p[3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
}

inline std::uint32_t getU32(const std::uint8_t* p) {
    return
        static_cast<std::uint32_t>(p[0]) |
        (static_cast<std::uint32_t>(p[1]) << 8) |
        (static_cast<std::uint32_t>(p[2]) << 16) |
        (static_cast<std::uint32_t>(p[3]) << 24);
}

inline void putFloat(std::uint8_t* p, float value) {
    static_assert(sizeof(float) == 4);

    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(float));

    putU32(p, bits);
}

inline float getFloat(const std::uint8_t* p) {
    std::uint32_t bits = getU32(p);

    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(float));

    return value;
}

} // namespace bptree::detail
