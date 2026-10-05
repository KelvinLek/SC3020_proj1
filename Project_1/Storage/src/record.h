// record.h - one NBA game (one row of games.txt) and its 23-byte on-disk form.
//
// Fixed format, fixed length (Lecture 03, case 1): every record has the same
// fields at the same offsets, so a record can be located by arithmetic alone.
//
//  offset size field            on disk
//  0      1    header           bit 7 = deleted (tombstone), bits 0-6 = null flags
//  1      2    GAME_DATE_EST    packed date: (year-2000)<<9 | month<<5 | day
//  3      4    TEAM_ID_home     uint32
//  7      1    PTS_home         uint8
//  8      4    FG_PCT_home      float (IEEE 754 single)  <- B+ tree key
//  12     4    FT_PCT_home      float
//  16     4    FG3_PCT_home     float
//  20     1    AST_home         uint8
//  21     1    REB_home         uint8
//  22     1    HOME_TEAM_WINS   uint8 (0/1)
//  = 23 bytes, fields packed with no alignment padding.
#pragma once
#include <cstdint>
#include <string>

#include "disk.h"

namespace storage {

constexpr std::size_t RECORD_SIZE = 23;

// Bits of the record header. A set null bit means the value was empty in games.txt.
enum NullBit : std::uint8_t {
    NULL_PTS = 1u << 0,
    NULL_FG_PCT = 1u << 1,
    NULL_FT_PCT = 1u << 2,
    NULL_FG3_PCT = 1u << 3,
    NULL_AST = 1u << 4,
    NULL_REB = 1u << 5,
    NULL_WINS = 1u << 6,
};
constexpr std::uint8_t DELETED_BIT = 1u << 7;

// In-memory view of a record.
struct Record {
    std::uint16_t gameDate = 0;   // packed, see packDate()
    std::uint32_t teamIdHome = 0;
    std::uint8_t ptsHome = 0;
    float fgPctHome = 0.f;
    float ftPctHome = 0.f;
    float fg3PctHome = 0.f;
    std::uint8_t astHome = 0;
    std::uint8_t rebHome = 0;
    std::uint8_t homeTeamWins = 0;
    std::uint8_t nullMask = 0;    // NullBit flags

    bool isNull(NullBit b) const { return (nullMask & b) != 0; }
    std::string dateString() const;   // "DD/MM/YYYY", as in games.txt
    std::string toString() const;     // one readable line, for demos
};

// Physical address of a record (Lecture 03): which block, which slot in it.
// This is the "pointer" a B+ tree leaf stores for each key: 6 bytes on disk.
struct RecordId {
    BlockId block = 0;
    std::uint16_t slot = 0;
};
constexpr std::size_t RECORD_ID_SIZE = 6;
void encodeRecordId(const RecordId& rid, std::uint8_t* out);   // writes 6 bytes
RecordId decodeRecordId(const std::uint8_t* in);

// Date packing: 7 bits year offset from 2000, 4 bits month, 5 bits day = 2 bytes
// instead of 10 characters ("Can we use fewer bytes?" - Lecture 03).
std::uint16_t packDate(int day, int month, int year);

// (De)serialisation between Record and its 23 on-disk bytes. Fields are copied
// one by one, so no compiler padding ends up on disk.
void serialize(const Record& r, std::uint8_t* out, bool deleted = false);
Record deserialize(const std::uint8_t* in);
bool isDeleted(const std::uint8_t* in);

// Parses one tab-separated data line of games.txt. `lineNo` is used in errors.
Record parseLine(const std::string& line, std::size_t lineNo);

}  // namespace storage
