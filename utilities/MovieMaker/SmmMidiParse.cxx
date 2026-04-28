#include "SmmMidiParse.hxx"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string_view>
#include <vector>

namespace Smm::Midi {
namespace {

static bool ReadU32BE(const uint8_t* p, uint32_t& o) {
    o = (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8)
        | static_cast<uint32_t>(p[3]);
    return true;
}

static bool ReadU16BE(const uint8_t* p, uint16_t& o) {
    o = static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
    return true;
}

static size_t ReadVarLen(const uint8_t* p, size_t maxN, uint32_t& outVal) {
    outVal = 0;
    for (size_t i = 0; i < 4 && i < maxN; ++i) {
        outVal = (outVal << 7) | static_cast<uint32_t>(p[i] & 0x7fu);
        if ((p[i] & 0x80u) == 0) {
            return i + 1;
        }
    }
    return 0;
}

static void BuildBeatGrid(uint32_t maxTick, uint32_t tpq, std::vector<uint32_t>& beats) {
    beats.clear();
    if (tpq == 0) {
        tpq = 480u;
    }
    for (uint32_t t = 0; t <= maxTick; t += tpq) {
        beats.push_back(t);
    }
    if (beats.empty()) {
        beats.push_back(0u);
    }
}

static bool ParseTrack(const uint8_t* d, size_t len, uint32_t& maxTickOut, uint32_t& uspqOut, bool& uspqSet) {
    size_t i = 0;
    uint32_t absTick = 0;
    uint8_t running = 0;

    while (i < len) {
        uint32_t dt = 0;
        const size_t adv = ReadVarLen(d + i, len - i, dt);
        if (adv == 0) {
            return false;
        }
        i += adv;
        absTick += dt;
        if (i >= len) {
            return false;
        }

        const uint8_t b0 = d[i];
        if (b0 >= 0x80u) {
            running = b0;
            ++i;
        } else {
            if (running == 0) {
                return false;
            }
        }

        if (running == 0xFFu) {
            if (i >= len) {
                return false;
            }
            const uint8_t meta = d[i++];
            uint32_t ml = 0;
            const size_t mlenAdv = ReadVarLen(d + i, len - i, ml);
            if (mlenAdv == 0 || i + mlenAdv + ml > len) {
                return false;
            }
            i += mlenAdv;
            if (meta == 0x2Fu) { // EOT
                break;
            }
            if (meta == 0x51u && ml == 3u) {
                const uint32_t us = (static_cast<uint32_t>(d[i]) << 16) | (static_cast<uint32_t>(d[i + 1]) << 8)
                    | static_cast<uint32_t>(d[i + 2]);
                uspqOut = us;
                uspqSet = true;
            }
            i += ml;
            running = 0;
            continue;
        }

        if (running == 0u) {
            return false;
        }

        const uint8_t kind = static_cast<uint8_t>(running & 0xF0u);
        if (kind == 0xF0u) {
            if (running == 0xF0u) {
                uint32_t syxN = 0;
                const size_t sla = ReadVarLen(d + i, len - i, syxN);
                if (sla == 0 || i + sla + syxN > len) {
                    return false;
                }
                i += sla + syxN;
            } else if (running == 0xF7u) {
                uint32_t sz = 0;
                const size_t sla = ReadVarLen(d + i, len - i, sz);
                if (sla == 0 || i + sla + sz > len) {
                    return false;
                }
                i += sla + sz;
            } else {
                return false;
            }
            running = 0;
            continue;
        }

        if (kind == 0xC0u || kind == 0xD0u) {
            if (i >= len) {
                return false;
            }
            i += 1;
            continue;
        }

        if (kind == 0x80u || kind == 0x90u || kind == 0xA0u || kind == 0xB0u || kind == 0xE0u) {
            if (i + 1 >= len) {
                return false;
            }
            const uint8_t b1 = d[i++];
            const uint8_t b2 = d[i++];
            (void)b1;
            if (kind == 0x90u && b2 > 0) {
                maxTickOut = (std::max)(maxTickOut, absTick);
            }
            continue;
        }

        return false;
    }
    return true;
}

} // namespace

bool ParseStandardMidiFile(const std::filesystem::path& path, ParseResult& out, std::string& err) {
    err.clear();
    out = {};
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        err = "Cannot open MIDI file.";
        return false;
    }
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (raw.size() < 14) {
        err = "File too small.";
        return false;
    }
    if (std::string_view(reinterpret_cast<const char*>(raw.data()), 4) != "MThd") {
        err = "Missing MThd header.";
        return false;
    }
    uint32_t hdrLen = 0;
    (void)ReadU32BE(raw.data() + 4, hdrLen);
    if (hdrLen < 6u || 8u + hdrLen > raw.size()) {
        err = "Bad MThd length.";
        return false;
    }
    uint16_t fmt = 0;
    uint16_t ntr = 0;
    uint16_t div = 0;
    (void)ReadU16BE(raw.data() + 8, fmt);
    (void)ReadU16BE(raw.data() + 10, ntr);
    (void)ReadU16BE(raw.data() + 12, div);
    (void)fmt;
    (void)ntr;
    out.Format = fmt;
    if ((div & 0x8000u) != 0) {
        err = "SMPTE time division not supported (use ticks-per-quarter files).";
        return false;
    }
    out.TicksPerQuarter = div == 0 ? 480u : static_cast<uint32_t>(div);

    size_t pos = 8u + static_cast<size_t>(hdrLen);
    uint32_t globalMax = 0u;
    uint32_t uspq = 500000u;
    bool uspqSet = false;

    while (pos + 8u <= raw.size()) {
        if (std::string_view(reinterpret_cast<const char*>(raw.data() + pos), 4) != "MTrk") {
            err = "Expected MTrk chunk.";
            return false;
        }
        uint32_t chLen = 0u;
        (void)ReadU32BE(raw.data() + pos + 4, chLen);
        pos += 8u;
        if (pos + chLen > raw.size()) {
            err = "Truncated MTrk.";
            return false;
        }
        uint32_t trMax = 0u;
        if (!ParseTrack(raw.data() + pos, chLen, trMax, uspq, uspqSet)) {
            err = "Track parse error.";
            return false;
        }
        globalMax = (std::max)(globalMax, trMax);
        pos += chLen;
    }

    out.MicrosecondsPerQuarter = uspq;
    out.MaxTick = globalMax;
    BuildBeatGrid(globalMax, out.TicksPerQuarter, out.BeatTicks);
    return true;
}

} // namespace Smm::Midi
