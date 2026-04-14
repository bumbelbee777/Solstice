#pragma once

#include "Solstice.hxx"
#include <string>
#include <vector>
#include <cstdint>

namespace Solstice::Core {

class SOLSTICE_API Base64 {
public:
    static std::string Encode(const uint8_t* Data, size_t Size);
    static std::string Encode(const std::vector<uint8_t>& Data) {
        return Encode(Data.data(), Data.size());
    }

    static std::vector<uint8_t> Decode(const std::string& String);

private:
    static constexpr char EncodingTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
};

} // namespace Solstice::Core
