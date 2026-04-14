#include "Fuzz.hxx"
#include <iostream>
#include "Core/Serialization/JSON.hxx"
#include "Asset/IO/WavefrontParser.hxx"

#include <filesystem>
#include <fstream>
#include <string>

#ifndef SOLSTICE_TEST_FIXTURES_DIR
#define SOLSTICE_TEST_FIXTURES_DIR "."
#endif

namespace {

void TryParseJson(const std::string& s) {
    try {
        (void)Solstice::Core::JSONParser::Parse(s);
    } catch (const std::exception&) {
        // Invalid JSON is expected under fuzzing
    }
}

int RunParserFuzz() {
    const int iterations = Solstice::Fuzz::IterationsFromEnv(120, 8000);
    const uint64_t seed = Solstice::Fuzz::SeedFromEnv();

    std::string baseJson = R"({"a":1,"b":[true,null],"c":"x"})";
    const std::filesystem::path fixtureDir(SOLSTICE_TEST_FIXTURES_DIR);
    const auto objPath = fixtureDir / "minimal.obj";
    std::string objTemplate;
    {
        std::ifstream in(objPath);
        if (in) {
            objTemplate.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
    }
    if (objTemplate.empty()) {
        objTemplate = "v 0 0 0\nf 1 1 1\n";
    }

    Solstice::Fuzz::Rng rng(seed);
    for (int i = 0; i < iterations; ++i) {
        std::string j = rng.MutateUtf8(baseJson, 4096);
        TryParseJson(j);

        std::vector<uint8_t> buf(objTemplate.begin(), objTemplate.end());
        rng.MutateBytes(buf, 8192);
        const auto tmp = std::filesystem::temp_directory_path()
            / ("solstice_fuzz_" + std::to_string(seed) + "_" + std::to_string(i) + ".obj");
        {
            std::ofstream out(tmp, std::ios::binary);
            if (out) {
                out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
            }
        }
        (void)Solstice::Core::WavefrontParser::Parse(tmp);
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
    }

    std::cout << "[ParserFuzzTest] iterations=" << iterations << " seed=" << seed << std::endl;
    return 0;
}

} // namespace

int main() {
    return RunParserFuzz();
}
