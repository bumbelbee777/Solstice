#include "BytecodeVM.hxx"
#include <cstring>
#include <variant>

namespace Solstice::Scripting {

struct ValueSerializer {
    std::vector<uint8_t>& Out;

    void operator()(int64_t v) {
        uint8_t type = 0; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(double v) {
        uint8_t type = 1; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(const std::string& v) {
        uint8_t type = 2; Out.push_back(type);
        uint32_t len = (uint32_t)v.size();
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&len);
        Out.insert(Out.end(), p, p + sizeof(len));
        Out.insert(Out.end(), v.begin(), v.end());
    }
    void operator()(const Solstice::Math::Vec2& v) {
        uint8_t type = 3; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(const Solstice::Math::Vec3& v) {
        uint8_t type = 4; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(const Solstice::Math::Vec4& v) {
        uint8_t type = 5; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(const Solstice::Math::Matrix2& v) {
        uint8_t type = 6; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(const Solstice::Math::Matrix3& v) {
        uint8_t type = 7; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(const Solstice::Math::Matrix4& v) {
        uint8_t type = 8; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(const Solstice::Math::Quaternion& v) {
        uint8_t type = 9; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(const std::shared_ptr<Array>& v) { uint8_t type = 10; Out.push_back(type); }
    void operator()(const std::shared_ptr<Dictionary>& v) { uint8_t type = 11; Out.push_back(type); }
    void operator()(const std::shared_ptr<Set>& v) { uint8_t type = 12; Out.push_back(type); }
    void operator()(const EnumVal& v) {
        uint8_t type = 13; Out.push_back(type);
        uint32_t len = (uint32_t)v.enumName.size();
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&len);
        Out.insert(Out.end(), p, p + sizeof(len));
        Out.insert(Out.end(), v.enumName.begin(), v.enumName.end());
        len = (uint32_t)v.variant.size();
        p = reinterpret_cast<const uint8_t*>(&len);
        Out.insert(Out.end(), p, p + sizeof(len));
        Out.insert(Out.end(), v.variant.begin(), v.variant.end());
        p = reinterpret_cast<const uint8_t*>(&v.discriminant);
        Out.insert(Out.end(), p, p + sizeof(v.discriminant));
    }
    void operator()(const ScriptFunc& v) {
        uint8_t type = 14; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v.entryIP);
        Out.insert(Out.end(), p, p + sizeof(v.entryIP));
        // capture not serialized (closure state lost on load)
    }

    void operator()(const PtrValue& v) {
        // PtrValue is not persisted with its payload; we only record a type tag
        // and reconstruct a null pointer on load. This is sufficient because
        // pointer values are not expected as immediate constants in bytecode.
        uint8_t type = 15;
        Out.push_back(type);
    }
};

void Program::Serialize(std::vector<uint8_t>& out) const {
    uint32_t count = (uint32_t)Instructions.size();
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&count);
    out.insert(out.end(), p, p + sizeof(count));

    for (const auto& inst : Instructions) {
        out.push_back((uint8_t)inst.Op);
        out.push_back(inst.RegisterIndex);
        std::visit(ValueSerializer{out}, inst.Operand);
    }

    // Serialize EnumInfo
    uint32_t enumCount = (uint32_t)EnumInfo.size();
    p = reinterpret_cast<const uint8_t*>(&enumCount);
    out.insert(out.end(), p, p + sizeof(enumCount));
    for (const auto& [enumName, meta] : EnumInfo) {
        uint32_t len = (uint32_t)enumName.size();
        p = reinterpret_cast<const uint8_t*>(&len);
        out.insert(out.end(), p, p + sizeof(len));
        out.insert(out.end(), enumName.begin(), enumName.end());
        uint32_t varCount = (uint32_t)meta.variantToDiscriminant.size();
        p = reinterpret_cast<const uint8_t*>(&varCount);
        out.insert(out.end(), p, p + sizeof(varCount));
        for (const auto& [varName, disc] : meta.variantToDiscriminant) {
            len = (uint32_t)varName.size();
            p = reinterpret_cast<const uint8_t*>(&len);
            out.insert(out.end(), p, p + sizeof(len));
            out.insert(out.end(), varName.begin(), varName.end());
            p = reinterpret_cast<const uint8_t*>(&disc);
            out.insert(out.end(), p, p + sizeof(disc));
        }
    }

    // Serialize FunctionArity
    uint32_t arityCount = (uint32_t)FunctionArity.size();
    p = reinterpret_cast<const uint8_t*>(&arityCount);
    out.insert(out.end(), p, p + sizeof(arityCount));
    for (const auto& [ip, ar] : FunctionArity) {
        p = reinterpret_cast<const uint8_t*>(&ip);
        out.insert(out.end(), p, p + sizeof(ip));
        p = reinterpret_cast<const uint8_t*>(&ar);
        out.insert(out.end(), p, p + sizeof(ar));
    }

    // Optional static-analysis metadata (ignored by older loaders)
    constexpr uint32_t kMetaMagic = 0x4D455441u; // 'META'
    uint32_t magic = kMetaMagic;
    p = reinterpret_cast<const uint8_t*>(&magic);
    out.insert(out.end(), p, p + sizeof(magic));

    uint32_t regTypeCount = (uint32_t)RegisterTypes.size();
    p = reinterpret_cast<const uint8_t*>(&regTypeCount);
    out.insert(out.end(), p, p + sizeof(regTypeCount));
    for (const auto& [reg, ty] : RegisterTypes) {
        out.push_back(reg);
        uint32_t len = (uint32_t)ty.size();
        p = reinterpret_cast<const uint8_t*>(&len);
        out.insert(out.end(), p, p + sizeof(len));
        out.insert(out.end(), ty.begin(), ty.end());
    }

    uint32_t ptrOpCount = (uint32_t)PtrOperandRegs.size();
    p = reinterpret_cast<const uint8_t*>(&ptrOpCount);
    out.insert(out.end(), p, p + sizeof(ptrOpCount));
    for (const auto& [ip, reg] : PtrOperandRegs) {
        uint64_t ip64 = (uint64_t)ip;
        p = reinterpret_cast<const uint8_t*>(&ip64);
        out.insert(out.end(), p, p + sizeof(ip64));
        out.push_back(reg);
    }
}

Program Program::Deserialize(const std::vector<uint8_t>& in) {
    Program prog;
    if (in.size() < 4) return prog;

    uint32_t count;
    std::memcpy(&count, in.data(), 4);
    size_t pos = 4;

    for (uint32_t i = 0; i < count; ++i) {
        if (pos >= in.size()) break;
        OpCode op = (OpCode)in[pos++];
        uint8_t reg = in[pos++];
        uint8_t type = in[pos++];

        Value val;
        if (type == 0) {
            int64_t v; std::memcpy(&v, &in[pos], 8); pos += 8; val = v;
        } else if (type == 1) {
            double v; std::memcpy(&v, &in[pos], 8); pos += 8; val = v;
        } else if (type == 2) {
            uint32_t len; std::memcpy(&len, &in[pos], 4); pos += 4;
            std::string s((const char*)&in[pos], len); pos += len; val = s;
        } else if (type == 3) {
            Solstice::Math::Vec2 v; std::memcpy(&v, &in[pos], sizeof(v)); pos += sizeof(v); val = v;
        } else if (type == 4) {
            Solstice::Math::Vec3 v; std::memcpy(&v, &in[pos], sizeof(v)); pos += sizeof(v); val = v;
        } else if (type == 5) {
            Solstice::Math::Vec4 v; std::memcpy(&v, &in[pos], sizeof(v)); pos += sizeof(v); val = v;
        } else if (type == 6) {
            Solstice::Math::Matrix2 v; std::memcpy(&v, &in[pos], sizeof(v)); pos += sizeof(v); val = v;
        } else if (type == 7) {
            Solstice::Math::Matrix3 v; std::memcpy(&v, &in[pos], sizeof(v)); pos += sizeof(v); val = v;
        } else if (type == 8) {
            Solstice::Math::Matrix4 v; std::memcpy(&v, &in[pos], sizeof(v)); pos += sizeof(v); val = v;
        } else if (type == 9) {
            Solstice::Math::Quaternion v; std::memcpy(&v, &in[pos], sizeof(v)); pos += sizeof(v); val = v;
        } else if (type == 13) {
            uint32_t len; std::memcpy(&len, &in[pos], 4); pos += 4;
            std::string enumName((const char*)&in[pos], len); pos += len;
            std::memcpy(&len, &in[pos], 4); pos += 4;
            std::string variant((const char*)&in[pos], len); pos += len;
            int64_t disc; std::memcpy(&disc, &in[pos], 8); pos += 8;
            val = EnumVal{enumName, variant, disc};
        } else if (type == 14) {
            size_t entryIP; std::memcpy(&entryIP, &in[pos], sizeof(entryIP)); pos += sizeof(entryIP);
            val = ScriptFunc{entryIP, nullptr};
        } else if (type == 15) {
            PtrValue ptr;
            val = ptr;
        }

        prog.Instructions.push_back({op, reg, val});
    }

    // Deserialize EnumInfo
    if (pos + sizeof(uint32_t) <= in.size()) {
        uint32_t enumCount;
        std::memcpy(&enumCount, &in[pos], 4); pos += 4;
        for (uint32_t e = 0; e < enumCount && pos < in.size(); ++e) {
            uint32_t len; std::memcpy(&len, &in[pos], 4); pos += 4;
            if (pos + len > in.size()) break;
            std::string enumName((const char*)&in[pos], len); pos += len;
            Program::EnumMetadata meta;
            uint32_t varCount; std::memcpy(&varCount, &in[pos], 4); pos += 4;
            for (uint32_t v = 0; v < varCount && pos < in.size(); ++v) {
                std::memcpy(&len, &in[pos], 4); pos += 4;
                if (pos + len > in.size()) break;
                std::string varName((const char*)&in[pos], len); pos += len;
                int64_t disc; std::memcpy(&disc, &in[pos], 8); pos += 8;
                meta.variantToDiscriminant[varName] = disc;
            }
            prog.EnumInfo[enumName] = meta;
        }
    }

    // Deserialize FunctionArity
    if (pos + sizeof(uint32_t) <= in.size()) {
        uint32_t arityCount;
        std::memcpy(&arityCount, &in[pos], 4); pos += 4;
        for (uint32_t a = 0; a < arityCount && pos + 8 + 8 <= in.size(); ++a) {
            size_t ip; std::memcpy(&ip, &in[pos], sizeof(ip)); pos += sizeof(ip);
            size_t ar; std::memcpy(&ar, &in[pos], sizeof(ar)); pos += sizeof(ar);
            prog.FunctionArity[ip] = ar;
        }
    }

    constexpr uint32_t kMetaMagic = 0x4D455441u;
    if (pos + sizeof(uint32_t) <= in.size()) {
        uint32_t mag;
        std::memcpy(&mag, &in[pos], 4);
        if (mag == kMetaMagic) {
            pos += 4;
            if (pos + sizeof(uint32_t) > in.size()) return prog;
            uint32_t regTypeCount;
            std::memcpy(&regTypeCount, &in[pos], 4); pos += 4;
            for (uint32_t i = 0; i < regTypeCount && pos < in.size(); ++i) {
                if (pos + 1 + sizeof(uint32_t) > in.size()) break;
                uint8_t reg = in[pos++];
                uint32_t len;
                std::memcpy(&len, &in[pos], 4); pos += 4;
                if (pos + len > in.size()) break;
                std::string ty((const char*)&in[pos], len); pos += len;
                prog.RegisterTypes[reg] = std::move(ty);
            }
            if (pos + sizeof(uint32_t) > in.size()) return prog;
            uint32_t ptrOpCount;
            std::memcpy(&ptrOpCount, &in[pos], 4); pos += 4;
            for (uint32_t i = 0; i < ptrOpCount && pos + sizeof(uint64_t) + 1 <= in.size(); ++i) {
                uint64_t ip64;
                std::memcpy(&ip64, &in[pos], sizeof(ip64)); pos += sizeof(ip64);
                uint8_t reg = in[pos++];
                prog.PtrOperandRegs[(size_t)ip64] = reg;
            }
        }
    }
    return prog;
}

}
