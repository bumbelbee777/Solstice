#include "LZX.hxx"
#include <cstring>
#include <algorithm>

namespace Solstice::Core {

namespace {
inline bool ReadLengthExtra(const std::byte*& src, const std::byte* end, uint32_t& out) {
    out = 0;
    uint8_t b;
    do {
        if (src >= end) return false;
        b = static_cast<uint8_t>(*src++);
        out += b;
    } while (b == 255);
    return true;
}
} // namespace

std::vector<std::byte> LZXDecompress(std::span<const std::byte> compressed, size_t uncompressedSize) {
    std::vector<std::byte> out(uncompressedSize);
    const size_t n = LZXDecompressInto(compressed, out);
    if (n != uncompressedSize) {
        return {};
    }
    return out;
}

size_t LZXDecompressInto(std::span<const std::byte> compressed, std::span<std::byte> output) {
    const std::byte* src = compressed.data();
    const std::byte* const srcEnd = compressed.data() + compressed.size();
    std::byte* dst = output.data();
    std::byte* const dstEnd = output.data() + output.size();

    while (src < srcEnd && dst < dstEnd) {
        uint8_t token = static_cast<uint8_t>(*src++);
        uint32_t literalLen = token >> 4;
        if (literalLen == 15) {
            uint32_t extra = 0;
            if (!ReadLengthExtra(src, srcEnd, extra)) return 0;
            literalLen += extra;
        }
        if (src + literalLen > srcEnd || dst + literalLen > dstEnd) return 0;
        std::memcpy(dst, src, literalLen);
        src += literalLen;
        dst += literalLen;
        if (src >= srcEnd) break;

        if (src + 2 > srcEnd) return 0;
        uint16_t offset = static_cast<uint16_t>(static_cast<uint8_t>(src[0]))
            | (static_cast<uint16_t>(static_cast<uint8_t>(src[1])) << 8);
        src += 2;
        if (offset == 0) break;

        uint32_t matchLen = (token & 0x0Fu) + 4;
        if (matchLen == 19) {
            uint32_t extra = 0;
            if (!ReadLengthExtra(src, srcEnd, extra)) return 0;
            matchLen += extra;
        }
        if (offset > static_cast<size_t>(dst - output.data())) return 0;
        std::byte* matchSrc = dst - offset;
        if (dst + matchLen > dstEnd) return 0;
        for (size_t i = 0; i < matchLen; ++i) {
            dst[i] = matchSrc[i];
        }
        dst += matchLen;
    }

    return static_cast<size_t>(dst - output.data());
}

std::vector<std::byte> LZXCompress(std::span<const std::byte> source) {
    std::vector<std::byte> out;
    const size_t n = source.size();
    if (n == 0) return out;

    const std::byte* src = source.data();
    auto append = [&out](const std::byte* p, size_t len) {
        for (size_t i = 0; i < len; ++i) out.push_back(p[i]);
    };
    auto appendByte = [&out](uint8_t b) { out.push_back(static_cast<std::byte>(b)); };

    constexpr int minMatch = 4;
    constexpr size_t hashBits = 14;
    constexpr size_t hashSize = 1u << hashBits;
    constexpr uint32_t hashMulti = 2654435761u;

    std::vector<int> hashHead(hashSize, -1);
    std::vector<int> hashNext(n, -1);

    auto hashFunc = [](const std::byte* p) -> size_t {
        uint32_t v;
        std::memcpy(&v, p, sizeof(uint32_t));
        return (v * hashMulti) >> (32 - hashBits);
    };

    size_t pos = 0;
    while (pos < n) {
        size_t literalStart = pos;
        while (pos < n) {
            if (pos + minMatch > n) {
                pos = n;
                break;
            }
            size_t h = hashFunc(src + pos);
            int bestLen = 0;
            int bestOff = 0;
            for (int q = hashHead[h]; q >= 0 && pos - q <= 65535; q = hashNext[q]) {
                int len = 0;
                while (pos + len < n && q + len < pos && src[pos + len] == src[q + len] && len < 65535) {
                    ++len;
                }
                if (len >= minMatch && len > bestLen) {
                    bestLen = len;
                    bestOff = static_cast<int>(pos - q);
                }
            }
            hashNext[pos] = hashHead[h];
            hashHead[h] = static_cast<int>(pos);

            if (bestLen >= minMatch) {
                size_t literalLen = pos - literalStart;
                uint8_t token = static_cast<uint8_t>(std::min<size_t>(literalLen, 15)) << 4;
                token |= static_cast<uint8_t>(std::min<size_t>(bestLen - 4, 15));
                appendByte(token);
                if (literalLen >= 15) {
                    size_t extra = literalLen - 15;
                    while (extra >= 255) { appendByte(255); extra -= 255; }
                    appendByte(static_cast<uint8_t>(extra));
                }
                append(src + literalStart, literalLen);
                appendByte(static_cast<uint8_t>(bestOff & 0xFF));
                appendByte(static_cast<uint8_t>(bestOff >> 8));
                if (bestLen - 4 >= 15) {
                    size_t extra = bestLen - 4 - 15;
                    while (extra >= 255) { appendByte(255); extra -= 255; }
                    appendByte(static_cast<uint8_t>(extra));
                }
                pos += static_cast<size_t>(bestLen);
                literalStart = pos;
                continue;
            }
            ++pos;
        }

        size_t literalLen = pos - literalStart;
        if (literalLen > 0) {
            appendByte(static_cast<uint8_t>(std::min<size_t>(literalLen, 15) << 4));
            if (literalLen >= 15) {
                size_t extra = literalLen - 15;
                while (extra >= 255) { appendByte(255); extra -= 255; }
                appendByte(static_cast<uint8_t>(extra));
            }
            append(src + literalStart, literalLen);
        }
    }

    appendByte(0);
    appendByte(0);
    return out;
}

} // namespace Solstice::Core
