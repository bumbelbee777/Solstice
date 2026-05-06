#include "TestHarness.hxx"
#include <Core/Audio/HRTF/HRTFDatabase.hxx>
#include <vector>

using namespace Solstice::Core::Audio;

int main() {
    HRTFDatabase db;
    TEST_ASSERT(db.LoadDefault());
    std::array<float, HRTF_FIR_LENGTH> l{};
    std::array<float, HRTF_FIR_LENGTH> r{};
    db.GetFilters(15.0f, 5.0f, l, r);
    TEST_ASSERT(l[0] != 0.0f || r[0] != 0.0f);

    std::vector<float> in(256, 0.0f);
    std::vector<float> out(256, 0.0f);
    in[0] = 1.0f;
    HRTFDatabase::ConvolveSIMD(in.data(), out.data(), l.data(), static_cast<int>(in.size()));
    TEST_ASSERT(out[0] != 0.0f);
    TEST_PASS();
}
