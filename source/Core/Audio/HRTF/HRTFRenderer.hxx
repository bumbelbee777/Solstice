#pragma once

#include "HRTFDatabase.hxx"
#include <Math/Quaternion.hxx>
#include <Math/Vector.hxx>
#include <unordered_map>

namespace Solstice::Core::Audio {

enum class AudioQualityLevel {
    Low,
    Medium,
    High
};

struct AudioBuffer {
    const float* Input{nullptr};
    float* OutputLeft{nullptr};
    float* OutputRight{nullptr};
    int SampleCount{0};
    uint32_t SourceId{0};
};

class HRTFRenderer {
public:
    HRTFRenderer() = default;
    ~HRTFRenderer() = default;

    void Initialize(int SampleRate, int MaxSources);
    void Shutdown();

    void SetEnabled(bool Enabled) { m_Enabled = Enabled; }
    bool IsEnabled() const { return m_Enabled && m_Database.IsValid(); }

    void Process(const float* Input, int SampleCount,
                 const Math::Vec3& SourcePos,
                 const Math::Vec3& ListenerPos,
                 const Math::Quaternion& ListenerRot,
                 float* OutputLeft, float* OutputRight);

    void ProcessBatch(const AudioBuffer* Inputs, int SourceCount,
                      const Math::Vec3* SourcePositions,
                      const Math::Vec3& ListenerPos,
                      const Math::Quaternion& ListenerRot,
                      AudioBuffer* Outputs);

    void CacheSource(uint32_t SourceId, const Math::Vec3& Position);
    bool GetCachedFilter(uint32_t SourceId, float* Left, float* Right);

    void SetQuality(AudioQualityLevel Quality) { m_Quality = Quality; }
    void SetHeadRadius(float Radius) { m_HeadRadius = std::max(0.03f, Radius); }
    bool LoadSOFA(const std::string& Path) { return m_Database.LoadSOFA(Path); }
    bool LoadDefaultDatabase() { return m_Database.LoadDefault(); }

private:
    struct CachedSource {
        Math::Vec3 LastPosition{0.0f, 0.0f, 0.0f};
        std::array<float, HRTF_FIR_LENGTH> LeftFilter{};
        std::array<float, HRTF_FIR_LENGTH> RightFilter{};
        int FramesSinceUpdate{0};
        bool Valid{false};
    };

    HRTFDatabase m_Database;
    std::unordered_map<uint32_t, CachedSource> m_SourceCache;
    int m_SampleRate{48000};
    int m_MaxSources{64};
    bool m_Enabled{false};
    AudioQualityLevel m_Quality{AudioQualityLevel::Medium};
    float m_HeadRadius{0.09f};

    static void ComputeRelativeDirection(const Math::Vec3& SourcePos,
                                         const Math::Vec3& ListenerPos,
                                         const Math::Quaternion& ListenerRot,
                                         float& Azimuth, float& Elevation);
};

} // namespace Solstice::Core::Audio
