#include "TestHarness.hxx"

#include <Core/System/LZX.hxx>
#include <cstddef>
#include <vector>

static bool Run() {
    using namespace Solstice::Core;
    std::vector<std::byte> source(4096);
    for (size_t i = 0; i < source.size(); ++i) {
        source[i] = static_cast<std::byte>(i & 0xFF);
    }

    std::vector<std::byte> compressed = LZXCompress(source);
    SOLSTICE_TEST_ASSERT(!compressed.empty(), "compressed data exists");
    std::vector<std::byte> decompressed = LZXDecompress(compressed, source.size());
    SOLSTICE_TEST_ASSERT(decompressed.size() == source.size(), "decompressed size matches");
    SOLSTICE_TEST_ASSERT(decompressed == source, "round-trip bytes match");
    SOLSTICE_TEST_PASS("LZX round-trip");
    return true;
}

int main() {
    if (!Run()) {
        return 1;
    }
    return SolsticeTestMainResult("LZXRoundTripTest");
}
