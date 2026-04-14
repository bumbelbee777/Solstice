#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Solstice::Scripting::Backend {

// Append-only x64 machine code buffer with a tiny peephole post-pass.
class CodeBuffer {
public:
    void EmitU8(uint8_t b) { m_Bytes.push_back(b); }
    void EmitBytes(const uint8_t* p, size_t n) {
        m_Bytes.insert(m_Bytes.end(), p, p + n);
    }

    void EmitMovRaxImm64(uint64_t imm) {
        // REX.W + B8 + imm64
        EmitU8(0x48);
        EmitU8(0xB8);
        for (int i = 0; i < 8; ++i) {
            EmitU8(static_cast<uint8_t>((imm >> (i * 8)) & 0xFF));
        }
    }

    void EmitMovRcxImm64(uint64_t imm) {
        EmitU8(0x48);
        EmitU8(0xB9);
        for (int i = 0; i < 8; ++i) {
            EmitU8(static_cast<uint8_t>((imm >> (i * 8)) & 0xFF));
        }
    }

    void EmitAddRaxRcx() {
        EmitU8(0x48);
        EmitU8(0x01);
        EmitU8(0xC8);
    }

    void EmitRet() { EmitU8(0xC3); }

    // Remove redundant mov rax, rax (48 89 C0) if present.
    void PeepholeMachine() {
        for (size_t i = 0; i + 3 <= m_Bytes.size(); ++i) {
            if (m_Bytes[i] == 0x48 && m_Bytes[i + 1] == 0x89 && m_Bytes[i + 2] == 0xC0) {
                m_Bytes.erase(m_Bytes.begin() + static_cast<std::ptrdiff_t>(i),
                              m_Bytes.begin() + static_cast<std::ptrdiff_t>(i + 3));
                if (i > 0) {
                    --i;
                }
            }
        }
    }

    const uint8_t* Data() const { return m_Bytes.data(); }
    size_t Size() const { return m_Bytes.size(); }
    std::vector<uint8_t>& Bytes() { return m_Bytes; }

private:
    std::vector<uint8_t> m_Bytes;
};

} // namespace Solstice::Scripting::Backend
