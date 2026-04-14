#include <Parallax/ParallaxEditOps.hxx>

#include <algorithm>
#include <cstdint>

namespace Solstice::Parallax {

void ShiftAllKeyframeTimes(ParallaxScene& scene, int64_t deltaTicks) {
    for (auto& ch : scene.GetChannels()) {
        for (auto& kf : ch.Keyframes) {
            int64_t t = static_cast<int64_t>(kf.TimeTicks) + deltaTicks;
            kf.TimeTicks = static_cast<uint64_t>(std::max<int64_t>(0, t));
        }
    }
    for (auto& tr : scene.GetMGTracks()) {
        for (auto& kf : tr.Keyframes) {
            int64_t t = static_cast<int64_t>(kf.TimeTicks) + deltaTicks;
            kf.TimeTicks = static_cast<uint64_t>(std::max<int64_t>(0, t));
        }
    }
}

} // namespace Solstice::Parallax
