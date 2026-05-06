#include "AmbisonicCodec.hxx"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Solstice::Core::Audio {

void AmbisonicCodec::SetOrder(AmbisonicOrder Order) {
    m_Order = Order;
    switch (m_Order) {
        case AmbisonicOrder::First: m_Channels = 4; break;
        case AmbisonicOrder::Second: m_Channels = 9; break;
        case AmbisonicOrder::Third: m_Channels = 16; break;
    }
    m_SHScratch.resize(static_cast<size_t>(m_Channels), 0.0f);
}

void AmbisonicCodec::ComputeSH(const Math::Vec3& Direction, float* OutSH) const {
    const Math::Vec3 n = (Direction.Dot(Direction) > 0.0001f) ? Direction.Normalized() : Math::Vec3(0.0f, 0.0f, -1.0f);
    const float x = n.x, y = n.y, z = n.z;
    if (m_Channels >= 1) OutSH[0] = 0.70710678f;
    if (m_Channels >= 2) OutSH[1] = y;
    if (m_Channels >= 3) OutSH[2] = z;
    if (m_Channels >= 4) OutSH[3] = x;
    for (int i = 4; i < m_Channels; ++i) {
        OutSH[i] = std::pow(x + 0.3f * y + 0.7f * z, static_cast<float>(1 + (i % 3)));
    }
}

void AmbisonicCodec::Encode(const float* Input, int SampleCount, const Math::Vec3& Direction, float* Output) const {
    if (!Input || !Output || SampleCount <= 0) return;
    std::vector<float> sh(static_cast<size_t>(m_Channels), 0.0f);
    ComputeSH(Direction, sh.data());
    for (int i = 0; i < SampleCount; ++i) {
        for (int c = 0; c < m_Channels; ++c) {
            Output[i * m_Channels + c] = Input[i] * sh[c];
        }
    }
}

void AmbisonicCodec::EncodeWithDistance(const float* Input, int SampleCount, const Math::Vec3& Direction,
                                        const Math::Vec3& ListenerPos, float* Output) const {
    if (!Input || !Output || SampleCount <= 0) return;
    const float dist = std::max(0.25f, (Direction - ListenerPos).Magnitude());
    const float gain = 1.0f / (1.0f + 0.25f * dist);
    std::vector<float> tmp(static_cast<size_t>(SampleCount), 0.0f);
    for (int i = 0; i < SampleCount; ++i) tmp[i] = Input[i] * gain;
    Encode(tmp.data(), SampleCount, Direction, Output);
}

void AmbisonicCodec::DecodeToBinaural(const float* Input, int SampleCount, const Math::Quaternion&,
                                      float* OutputLeft, float* OutputRight) const {
    if (!Input || !OutputLeft || !OutputRight || SampleCount <= 0) return;
    for (int i = 0; i < SampleCount; ++i) {
        const float w = Input[i * m_Channels + 0];
        const float x = (m_Channels > 3) ? Input[i * m_Channels + 3] : 0.0f;
        const float y = (m_Channels > 1) ? Input[i * m_Channels + 1] : 0.0f;
        OutputLeft[i] = w + 0.35f * x - 0.25f * y;
        OutputRight[i] = w - 0.35f * x - 0.25f * y;
    }
}

void AmbisonicCodec::DecodeToSpeakers(const float* Input, int SampleCount, int SpeakerLayout, float* Output) const {
    if (!Input || !Output || SampleCount <= 0) return;
    const int channels = std::max(2, SpeakerLayout);
    for (int i = 0; i < SampleCount; ++i) {
        for (int c = 0; c < channels; ++c) {
            Output[i * channels + c] = Input[i * m_Channels + (c % m_Channels)];
        }
    }
}

void AmbisonicCodec::RotateSoundfield(const float* Input, float* Output, const Math::Quaternion&) const {
    if (!Input || !Output) return;
    // Minimal stable pass-through; rotation refinement can be layered later.
    std::memcpy(Output, Input, sizeof(float) * static_cast<size_t>(m_Channels));
}

} // namespace Solstice::Core::Audio
