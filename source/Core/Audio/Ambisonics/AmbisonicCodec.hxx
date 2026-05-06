#pragma once

#include <Math/Quaternion.hxx>
#include <Math/Vector.hxx>
#include <vector>

namespace Solstice::Core::Audio {

enum class AmbisonicOrder {
    First,
    Second,
    Third
};

class AmbisonicCodec {
public:
    void SetOrder(AmbisonicOrder Order);
    AmbisonicOrder GetOrder() const { return m_Order; }
    int GetChannels() const { return m_Channels; }

    void Encode(const float* Input, int SampleCount, const Math::Vec3& Direction, float* Output) const;
    void EncodeWithDistance(const float* Input, int SampleCount, const Math::Vec3& Direction,
                            const Math::Vec3& ListenerPos, float* Output) const;
    void DecodeToBinaural(const float* Input, int SampleCount, const Math::Quaternion& ListenerRot,
                          float* OutputLeft, float* OutputRight) const;
    void DecodeToSpeakers(const float* Input, int SampleCount, int SpeakerLayout, float* Output) const;
    void RotateSoundfield(const float* Input, float* Output, const Math::Quaternion& Rotation) const;

private:
    AmbisonicOrder m_Order{AmbisonicOrder::First};
    int m_Channels{4};
    std::vector<float> m_SHScratch;

    void ComputeSH(const Math::Vec3& Direction, float* OutSH) const;
};

} // namespace Solstice::Core::Audio
