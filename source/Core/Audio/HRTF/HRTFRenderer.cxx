#include "HRTFRenderer.hxx"
#include <algorithm>
#include <cmath>

namespace Solstice::Core::Audio {

void HRTFRenderer::Initialize(int SampleRate, int MaxSources) {
    m_SampleRate = std::max(8000, SampleRate);
    m_MaxSources = std::max(1, MaxSources);
    m_SourceCache.reserve(static_cast<size_t>(m_MaxSources));
    if (!m_Database.IsValid()) {
        m_Database.LoadDefault();
    }
    m_Enabled = true;
}

void HRTFRenderer::Shutdown() {
    m_SourceCache.clear();
    m_Enabled = false;
}

void HRTFRenderer::ComputeRelativeDirection(const Math::Vec3& SourcePos,
                                            const Math::Vec3& ListenerPos,
                                            const Math::Quaternion&,
                                            float& Azimuth, float& Elevation) {
    const Math::Vec3 d = SourcePos - ListenerPos;
    const float mag = std::max(0.0001f, d.Magnitude());
    const Math::Vec3 n = d / mag;
    Azimuth = std::atan2(n.x, -n.z) * (180.0f / 3.14159265358979323846f);
    Elevation = std::asin(std::clamp(n.y, -1.0f, 1.0f)) * (180.0f / 3.14159265358979323846f);
}

void HRTFRenderer::Process(const float* Input, int SampleCount,
                           const Math::Vec3& SourcePos,
                           const Math::Vec3& ListenerPos,
                           const Math::Quaternion& ListenerRot,
                           float* OutputLeft, float* OutputRight) {
    if (!Input || !OutputLeft || !OutputRight || SampleCount <= 0) {
        return;
    }
    if (!IsEnabled()) {
        for (int i = 0; i < SampleCount; ++i) {
            OutputLeft[i] = Input[i];
            OutputRight[i] = Input[i];
        }
        return;
    }
    float az = 0.0f, el = 0.0f;
    ComputeRelativeDirection(SourcePos, ListenerPos, ListenerRot, az, el);
    std::array<float, HRTF_FIR_LENGTH> left{};
    std::array<float, HRTF_FIR_LENGTH> right{};
    m_Database.GetFilters(az, el, left, right);
    HRTFDatabase::ConvolveSIMD(Input, OutputLeft, left.data(), SampleCount);
    HRTFDatabase::ConvolveSIMD(Input, OutputRight, right.data(), SampleCount);
}

void HRTFRenderer::ProcessBatch(const AudioBuffer* Inputs, int SourceCount,
                                const Math::Vec3* SourcePositions,
                                const Math::Vec3& ListenerPos,
                                const Math::Quaternion& ListenerRot,
                                AudioBuffer* Outputs) {
    if (!Inputs || !Outputs || !SourcePositions || SourceCount <= 0) {
        return;
    }
    for (int i = 0; i < SourceCount; ++i) {
        Outputs[i] = Inputs[i];
        Process(Inputs[i].Input, Inputs[i].SampleCount, SourcePositions[i], ListenerPos, ListenerRot,
                Outputs[i].OutputLeft, Outputs[i].OutputRight);
    }
}

void HRTFRenderer::CacheSource(uint32_t SourceId, const Math::Vec3& Position) {
    if (SourceId == 0) {
        return;
    }
    auto& cached = m_SourceCache[SourceId];
    cached.LastPosition = Position;
    float az = 0.0f, el = 0.0f;
    ComputeRelativeDirection(Position, Math::Vec3(0.0f, 0.0f, 0.0f), Math::Quaternion(), az, el);
    m_Database.GetFilters(az, el, cached.LeftFilter, cached.RightFilter);
    cached.FramesSinceUpdate = 0;
    cached.Valid = true;
}

bool HRTFRenderer::GetCachedFilter(uint32_t SourceId, float* Left, float* Right) {
    const auto it = m_SourceCache.find(SourceId);
    if (it == m_SourceCache.end() || !it->second.Valid || !Left || !Right) {
        return false;
    }
    std::copy(it->second.LeftFilter.begin(), it->second.LeftFilter.end(), Left);
    std::copy(it->second.RightFilter.begin(), it->second.RightFilter.end(), Right);
    return true;
}

} // namespace Solstice::Core::Audio
