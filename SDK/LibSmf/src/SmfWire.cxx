#include <Smf/SmfWire.hxx>

#include <cstring>
#include <string>

namespace Solstice::Smf::Wire {

void AppendU32(std::vector<std::byte>& b, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        b.push_back(static_cast<std::byte>(static_cast<uint8_t>(v & 0xFF)));
        v >>= 8;
    }
}

void AppendU64(std::vector<std::byte>& b, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        b.push_back(static_cast<std::byte>(static_cast<uint8_t>(v & 0xFF)));
        v >>= 8;
    }
}

uint32_t ReadU32(const std::byte* p) {
    uint32_t v = 0;
    std::memcpy(&v, p, 4);
    return v;
}

uint64_t ReadU64(const std::byte* p) {
    uint64_t v = 0;
    std::memcpy(&v, p, 8);
    return v;
}

void WriteAttributeValue(std::vector<std::byte>& out, SmfAttributeType t, const SmfValue& v) {
    auto w32 = [&](uint32_t x) { AppendU32(out, x); };
    auto wf = [&](float x) {
        out.insert(out.end(), 4, std::byte{0});
        std::memcpy(out.data() + out.size() - 4, &x, 4);
    };

    switch (t) {
    case SmfAttributeType::Bool:
        out.push_back(std::byte{static_cast<uint8_t>(std::get_if<bool>(&v) && *std::get_if<bool>(&v) ? 1 : 0)});
        break;
    case SmfAttributeType::Int32:
        w32(std::get_if<int32_t>(&v) ? static_cast<uint32_t>(*std::get_if<int32_t>(&v)) : 0u);
        break;
    case SmfAttributeType::Int64: {
        int64_t x = std::get_if<int64_t>(&v) ? *std::get_if<int64_t>(&v) : 0;
        AppendU64(out, static_cast<uint64_t>(x));
        break;
    }
    case SmfAttributeType::Float:
        wf(std::get_if<float>(&v) ? *std::get_if<float>(&v) : 0.f);
        break;
    case SmfAttributeType::Double: {
        double x = std::get_if<double>(&v) ? *std::get_if<double>(&v) : 0.0;
        uint64_t bits{};
        std::memcpy(&bits, &x, sizeof(bits));
        AppendU64(out, bits);
        break;
    }
    case SmfAttributeType::Vec2: {
        SmfVec2 a = std::get_if<SmfVec2>(&v) ? *std::get_if<SmfVec2>(&v) : SmfVec2{};
        wf(a.x);
        wf(a.y);
        break;
    }
    case SmfAttributeType::Vec3: {
        SmfVec3 a = std::get_if<SmfVec3>(&v) ? *std::get_if<SmfVec3>(&v) : SmfVec3{};
        wf(a.x);
        wf(a.y);
        wf(a.z);
        break;
    }
    case SmfAttributeType::Vec4:
    case SmfAttributeType::ColorRGBA: {
        SmfVec4 a = std::get_if<SmfVec4>(&v) ? *std::get_if<SmfVec4>(&v) : SmfVec4{};
        wf(a.x);
        wf(a.y);
        wf(a.z);
        wf(a.w);
        break;
    }
    case SmfAttributeType::Quaternion: {
        SmfQuaternion q = std::get_if<SmfQuaternion>(&v) ? *std::get_if<SmfQuaternion>(&v) : SmfQuaternion{};
        wf(q.x);
        wf(q.y);
        wf(q.z);
        wf(q.w);
        break;
    }
    case SmfAttributeType::Matrix4: {
        SmfMatrix4 m = std::get_if<SmfMatrix4>(&v) ? *std::get_if<SmfMatrix4>(&v) : SmfMatrix4{};
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                wf(m.m[static_cast<size_t>(r)][static_cast<size_t>(c)]);
            }
        }
        break;
    }
    case SmfAttributeType::String: {
        const std::string& s = std::get_if<std::string>(&v) ? *std::get_if<std::string>(&v) : std::string{};
        w32(static_cast<uint32_t>(s.size()));
        for (unsigned char ch : s) {
            out.push_back(static_cast<std::byte>(ch));
        }
        break;
    }
    case SmfAttributeType::AssetHash:
    case SmfAttributeType::ArzachelSeed:
        AppendU64(out, std::get_if<uint64_t>(&v) ? *std::get_if<uint64_t>(&v) : 0ull);
        break;
    case SmfAttributeType::BlobOpaque:
    case SmfAttributeType::SkeletonPose: {
        const auto& blob = std::get_if<std::vector<std::byte>>(&v) ? *std::get_if<std::vector<std::byte>>(&v)
                                                                   : std::vector<std::byte>{};
        w32(static_cast<uint32_t>(blob.size()));
        out.insert(out.end(), blob.begin(), blob.end());
        break;
    }
    case SmfAttributeType::ElementRef:
        w32(std::get_if<uint32_t>(&v) ? *std::get_if<uint32_t>(&v) : SMF_INVALID_INDEX);
        break;
    case SmfAttributeType::EasingType:
    case SmfAttributeType::TransitionBlendMode:
        out.push_back(std::byte{static_cast<uint8_t>(std::get_if<uint8_t>(&v) ? *std::get_if<uint8_t>(&v) : 0)});
        break;
    default:
        break;
    }
}

bool ReadAttributeValue(const std::byte*& p, const std::byte* end, SmfAttributeType t, SmfValue& outVal) {
    auto need = [&](size_t n) -> bool { return static_cast<size_t>(end - p) >= n; };
    auto r32 = [&]() -> uint32_t {
        uint32_t v = ReadU32(p);
        p += 4;
        return v;
    };
    auto rf = [&]() -> float {
        float x;
        std::memcpy(&x, p, 4);
        p += 4;
        return x;
    };

    switch (t) {
    case SmfAttributeType::Bool:
        if (!need(1)) {
            return false;
        }
        outVal = static_cast<bool>(static_cast<uint8_t>(*p++));
        break;
    case SmfAttributeType::Int32:
        if (!need(4)) {
            return false;
        }
        outVal = static_cast<int32_t>(r32());
        break;
    case SmfAttributeType::Int64:
        if (!need(8)) {
            return false;
        }
        outVal = static_cast<int64_t>(ReadU64(p));
        p += 8;
        break;
    case SmfAttributeType::Float:
        if (!need(4)) {
            return false;
        }
        outVal = rf();
        break;
    case SmfAttributeType::Double:
        if (!need(8)) {
            return false;
        }
        {
            double d;
            std::memcpy(&d, p, 8);
            p += 8;
            outVal = d;
        }
        break;
    case SmfAttributeType::Vec2:
        if (!need(8)) {
            return false;
        }
        outVal = SmfVec2{rf(), rf()};
        break;
    case SmfAttributeType::Vec3:
        if (!need(12)) {
            return false;
        }
        outVal = SmfVec3{rf(), rf(), rf()};
        break;
    case SmfAttributeType::Vec4:
    case SmfAttributeType::ColorRGBA:
        if (!need(16)) {
            return false;
        }
        outVal = SmfVec4{rf(), rf(), rf(), rf()};
        break;
    case SmfAttributeType::Quaternion:
        if (!need(16)) {
            return false;
        }
        outVal = SmfQuaternion{rf(), rf(), rf(), rf()};
        break;
    case SmfAttributeType::Matrix4:
        if (!need(64)) {
            return false;
        }
        {
            SmfMatrix4 m{};
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) {
                    m.m[static_cast<size_t>(r)][static_cast<size_t>(c)] = rf();
                }
            }
            outVal = m;
        }
        break;
    case SmfAttributeType::String: {
        if (!need(4)) {
            return false;
        }
        uint32_t len = r32();
        if (!need(len)) {
            return false;
        }
        std::string s(len, '\0');
        std::memcpy(s.data(), p, len);
        p += len;
        outVal = std::move(s);
        break;
    }
    case SmfAttributeType::AssetHash:
    case SmfAttributeType::ArzachelSeed:
        if (!need(8)) {
            return false;
        }
        outVal = ReadU64(p);
        p += 8;
        break;
    case SmfAttributeType::BlobOpaque:
    case SmfAttributeType::SkeletonPose:
        if (!need(4)) {
            return false;
        }
        {
            uint32_t len = r32();
            if (!need(len)) {
                return false;
            }
            std::vector<std::byte> blob(len);
            std::memcpy(blob.data(), p, len);
            p += len;
            outVal = std::move(blob);
        }
        break;
    case SmfAttributeType::ElementRef:
        if (!need(4)) {
            return false;
        }
        outVal = r32();
        break;
    case SmfAttributeType::EasingType:
    case SmfAttributeType::TransitionBlendMode:
        if (!need(1)) {
            return false;
        }
        outVal = static_cast<uint8_t>(static_cast<uint8_t>(*p++));
        break;
    default:
        outVal = std::monostate{};
        break;
    }
    return true;
}

} // namespace Solstice::Smf::Wire
