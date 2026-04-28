#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Smm::Midi {

/// Summary of a Standard MIDI File (SMF0/SMF1) for SMM conductor / beat grid.
struct ParseResult {
    uint16_t Format{0};
    uint32_t TicksPerQuarter{480};
    uint32_t MicrosecondsPerQuarter{500000u};
    uint32_t MaxTick{0};
    /// Quarter-note grid in MIDI ticks (0, tpq, 2*tpq, …) up to MaxTick.
    std::vector<uint32_t> BeatTicks;
};

bool ParseStandardMidiFile(const std::filesystem::path& path, ParseResult& out, std::string& err);

} // namespace Smm::Midi
