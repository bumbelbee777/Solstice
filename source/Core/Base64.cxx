#include "Base64.hxx"
#include <stdexcept>

namespace Solstice::Core {

std::string Base64::Encode(const uint8_t* Data, size_t Size) {
    std::string Out;
    Out.reserve(((Size + 2) / 3) * 4);

    uint32_t Val = 0;
    int ValB = -6;
    for (size_t i = 0; i < Size; i++) {
        Val = (Val << 8) + Data[i];
        ValB += 8;
        while (ValB >= 0) {
            Out.push_back(EncodingTable[(Val >> ValB) & 0x3F]);
            ValB -= 6;
        }
    }

    if (ValB > -6) {
        Out.push_back(EncodingTable[((Val << 8) >> (ValB + 8)) & 0x3F]);
    }

    while (Out.size() % 4) {
        Out.push_back('=');
    }

    return Out;
}

std::vector<uint8_t> Base64::Decode(const std::string& String) {
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[EncodingTable[i]] = i;

    std::vector<uint8_t> Out;
    int Val = 0;
    int ValB = -8;
    for (unsigned char C : String) {
        if (T[C] == -1) break;
        Val = (Val << 6) + T[C];
        ValB += 6;
        if (ValB >= 0) {
            Out.push_back(static_cast<uint8_t>((Val >> ValB) & 0xFF));
            ValB -= 8;
        }
    }
    return Out;
}

} // namespace Solstice::Core
