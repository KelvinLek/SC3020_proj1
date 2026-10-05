#include "record.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace storage {

std::uint16_t packDate(int day, int month, int year) {
    if (year < 2000 || year > 2127 || month < 1 || month > 12 || day < 1 || day > 31)
        throw std::runtime_error("date out of supported range");
    return static_cast<std::uint16_t>(((year - 2000) << 9) | (month << 5) | day);
}

std::string Record::dateString() const {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d/%02d/%04d", gameDate & 0x1F, (gameDate >> 5) & 0x0F,
                  (gameDate >> 9) + 2000);
    return buf;
}

std::string Record::toString() const {
    auto f = [&](NullBit b, float v) {
        if (isNull(b)) return std::string("NULL");
        char s[16];
        std::snprintf(s, sizeof s, "%.3f", v);
        return std::string(s);
    };
    auto u = [&](NullBit b, unsigned v) { return isNull(b) ? std::string("NULL") : std::to_string(v); };
    std::ostringstream os;
    os << dateString() << "  team=" << teamIdHome << "  pts=" << u(NULL_PTS, ptsHome)
       << "  fg%=" << f(NULL_FG_PCT, fgPctHome) << "  ft%=" << f(NULL_FT_PCT, ftPctHome)
       << "  fg3%=" << f(NULL_FG3_PCT, fg3PctHome) << "  ast=" << u(NULL_AST, astHome)
       << "  reb=" << u(NULL_REB, rebHome) << "  win=" << u(NULL_WINS, homeTeamWins);
    return os.str();
}

void encodeRecordId(const RecordId& rid, std::uint8_t* out) {
    std::memcpy(out, &rid.block, 4);
    std::memcpy(out + 4, &rid.slot, 2);
}

RecordId decodeRecordId(const std::uint8_t* in) {
    RecordId rid;
    std::memcpy(&rid.block, in, 4);
    std::memcpy(&rid.slot, in + 4, 2);
    return rid;
}

void serialize(const Record& r, std::uint8_t* out, bool deleted) {
    out[0] = static_cast<std::uint8_t>((r.nullMask & 0x7F) | (deleted ? DELETED_BIT : 0));
    std::memcpy(out + 1, &r.gameDate, 2);
    std::memcpy(out + 3, &r.teamIdHome, 4);
    out[7] = r.ptsHome;
    std::memcpy(out + 8, &r.fgPctHome, 4);
    std::memcpy(out + 12, &r.ftPctHome, 4);
    std::memcpy(out + 16, &r.fg3PctHome, 4);
    out[20] = r.astHome;
    out[21] = r.rebHome;
    out[22] = r.homeTeamWins;
}

Record deserialize(const std::uint8_t* in) {
    Record r;
    r.nullMask = in[0] & 0x7F;
    std::memcpy(&r.gameDate, in + 1, 2);
    std::memcpy(&r.teamIdHome, in + 3, 4);
    r.ptsHome = in[7];
    std::memcpy(&r.fgPctHome, in + 8, 4);
    std::memcpy(&r.ftPctHome, in + 12, 4);
    std::memcpy(&r.fg3PctHome, in + 16, 4);
    r.astHome = in[20];
    r.rebHome = in[21];
    r.homeTeamWins = in[22];
    return r;
}

bool isDeleted(const std::uint8_t* in) { return (in[0] & DELETED_BIT) != 0; }

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (true) {
        std::size_t tab = line.find('\t', start);
        out.push_back(line.substr(start, tab == std::string::npos ? std::string::npos : tab - start));
        if (tab == std::string::npos) break;
        start = tab + 1;
    }
    return out;
}

[[noreturn]] void fail(std::size_t lineNo, const std::string& what) {
    throw std::runtime_error("games.txt line " + std::to_string(lineNo) + ": " + what);
}

// Parses an unsigned integer that must fit in `maxValue`; empty -> null.
bool parseUInt(const std::string& s, unsigned long maxValue, unsigned long& out, std::size_t lineNo,
               const char* name) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtoul(s.c_str(), &end, 10);
    if (*end != '\0') fail(lineNo, std::string("bad integer in ") + name + ": '" + s + "'");
    if (out > maxValue) fail(lineNo, std::string(name) + " too large for its field: " + s);
    return true;
}

bool parseFloat(const std::string& s, float& out, std::size_t lineNo, const char* name) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtof(s.c_str(), &end);
    if (*end != '\0') fail(lineNo, std::string("bad number in ") + name + ": '" + s + "'");
    return true;
}

}  // namespace

Record parseLine(const std::string& rawLine, std::size_t lineNo) {
    std::string line = rawLine;
    if (!line.empty() && line.back() == '\r') line.pop_back();   // Windows line endings
    std::vector<std::string> c = splitTabs(line);
    if (c.size() != 9) fail(lineNo, "expected 9 tab-separated columns, got " + std::to_string(c.size()));

    Record r;
    int d = 0, m = 0, y = 0;
    if (std::sscanf(c[0].c_str(), "%d/%d/%d", &d, &m, &y) != 3) fail(lineNo, "bad date '" + c[0] + "'");
    try {
        r.gameDate = packDate(d, m, y);
    } catch (const std::exception&) {
        fail(lineNo, "date out of range '" + c[0] + "'");
    }

    unsigned long v = 0;
    if (!parseUInt(c[1], 0xFFFFFFFFul, v, lineNo, "TEAM_ID_home")) fail(lineNo, "missing TEAM_ID_home");
    r.teamIdHome = static_cast<std::uint32_t>(v);
    if (parseUInt(c[2], 255, v, lineNo, "PTS_home")) r.ptsHome = static_cast<std::uint8_t>(v); else r.nullMask |= NULL_PTS;
    if (!parseFloat(c[3], r.fgPctHome, lineNo, "FG_PCT_home")) r.nullMask |= NULL_FG_PCT;
    if (!parseFloat(c[4], r.ftPctHome, lineNo, "FT_PCT_home")) r.nullMask |= NULL_FT_PCT;
    if (!parseFloat(c[5], r.fg3PctHome, lineNo, "FG3_PCT_home")) r.nullMask |= NULL_FG3_PCT;
    if (parseUInt(c[6], 255, v, lineNo, "AST_home")) r.astHome = static_cast<std::uint8_t>(v); else r.nullMask |= NULL_AST;
    if (parseUInt(c[7], 255, v, lineNo, "REB_home")) r.rebHome = static_cast<std::uint8_t>(v); else r.nullMask |= NULL_REB;
    if (parseUInt(c[8], 1, v, lineNo, "HOME_TEAM_WINS")) r.homeTeamWins = static_cast<std::uint8_t>(v); else r.nullMask |= NULL_WINS;
    return r;
}

}  // namespace storage
