#pragma once

#include "LibUI/Core/Core.hxx"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace LibUI::Timeline {

struct Track {
    std::string name;
    bool expanded{true};
    std::vector<uint64_t> keyTicks;
};

struct TimelineState {
    uint64_t durationTicks{1};
    uint64_t playheadTick{0};
    float zoom{1.0f};
    float rowHeight{24.0f};
    float labelWidth{180.0f};
    std::vector<Track> tracks;
    int selectedTrack{-1};
    int selectedKey{-1};
    bool followPlayhead{true};
    /// When set, the track area maps **[nestedRangeStartTick, nestedRangeEndTick)** to the full width (nested “shot” / sub-timeline). Ruler and scrubbing use **scene** ticks; keys outside the range are drawn at the edges.
    bool nestedViewEnabled{false};
    uint64_t nestedRangeStartTick{0};
    uint64_t nestedRangeEndTick{1};
};

/** Ensures `nestedRangeEndTick > nestedRangeStartTick` and both lie within [0, durationTicks]. */
inline void TimelineClampNestedRange(TimelineState& s) {
    s.durationTicks = (std::max)(s.durationTicks, uint64_t{1});
    if (s.nestedRangeStartTick >= s.durationTicks) {
        s.nestedRangeStartTick = 0;
    }
    if (s.nestedRangeEndTick > s.durationTicks) {
        s.nestedRangeEndTick = s.durationTicks;
    }
    if (s.nestedRangeEndTick <= s.nestedRangeStartTick) {
        if (s.nestedRangeStartTick + 1 <= s.durationTicks) {
            s.nestedRangeEndTick = s.nestedRangeStartTick + 1;
        } else {
            s.nestedRangeStartTick = 0;
            s.nestedRangeEndTick = 1;
        }
    }
}

} // namespace LibUI::Timeline

