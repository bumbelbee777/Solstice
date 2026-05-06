#include "TestHarness.hxx"
#include <Core/Audio/Ambisonics/AmbisonicCodec.hxx>
#include <vector>

using namespace Solstice::Core::Audio;

int main() {
    AmbisonicCodec codec;
    codec.SetOrder(AmbisonicOrder::Second);
    TEST_ASSERT(codec.GetChannels() == 9);

    std::vector<float> input(64, 0.5f);
    std::vector<float> encoded(static_cast<size_t>(64 * codec.GetChannels()), 0.0f);
    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);

    codec.Encode(input.data(), 64, Math::Vec3(0.0f, 0.0f, -1.0f), encoded.data());
    codec.DecodeToBinaural(encoded.data(), 64, Math::Quaternion(), left.data(), right.data());
    TEST_ASSERT(left[0] != 0.0f || right[0] != 0.0f);
    TEST_PASS();
}
