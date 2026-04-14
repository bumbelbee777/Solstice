#pragma once

#include "SmfTypes.hxx"

#include <cstring>
#include <string>

namespace Solstice::Smf {

inline const char* SmfErrorMessage(SmfError e) {
    switch (e) {
    case SmfError::None:
        return "No error";
    case SmfError::InvalidMagic:
        return "Invalid file magic (not an SMF file)";
    case SmfError::UnsupportedVersion:
        return "Unsupported format version";
    case SmfError::CorruptHeader:
        return "Corrupt or truncated file header";
    case SmfError::CorruptSection:
        return "Corrupt section data";
    case SmfError::CorruptStringTable:
        return "Corrupt string table";
    case SmfError::CorruptEntitySection:
        return "Corrupt entity section";
    case SmfError::OutOfMemory:
        return "Out of memory";
    case SmfError::IoOpenFailed:
        return "Failed to open file";
    case SmfError::IoReadFailed:
        return "Failed to read file";
    case SmfError::IoWriteFailed:
        return "Failed to write file";
    default:
        return "Unknown error";
    }
}

/// Short labels for editors and debug output (matches on-disk type ids).
inline const char* SmfAttributeTypeLabel(SmfAttributeType t) {
    switch (t) {
    case SmfAttributeType::Bool:
        return "bool";
    case SmfAttributeType::Int32:
        return "int32";
    case SmfAttributeType::Int64:
        return "int64";
    case SmfAttributeType::Float:
        return "float";
    case SmfAttributeType::Double:
        return "double";
    case SmfAttributeType::Vec2:
        return "vec2";
    case SmfAttributeType::Vec3:
        return "vec3";
    case SmfAttributeType::Vec4:
        return "vec4";
    case SmfAttributeType::Quaternion:
        return "quaternion";
    case SmfAttributeType::Matrix4:
        return "matrix4";
    case SmfAttributeType::String:
        return "string";
    case SmfAttributeType::AssetHash:
        return "assetHash";
    case SmfAttributeType::BlobOpaque:
        return "blob";
    case SmfAttributeType::ElementRef:
        return "elementRef";
    case SmfAttributeType::ArzachelSeed:
        return "arzachelSeed";
    case SmfAttributeType::SkeletonPose:
        return "skeletonPose";
    case SmfAttributeType::EasingType:
        return "easingType";
    case SmfAttributeType::ColorRGBA:
        return "colorRGBA";
    case SmfAttributeType::TransitionBlendMode:
        return "blendMode";
    default:
        return "?";
    }
}

inline SmfValue SmfDefaultValueForType(SmfAttributeType t) {
    switch (t) {
    case SmfAttributeType::Bool:
        return false;
    case SmfAttributeType::Int32:
        return int32_t{0};
    case SmfAttributeType::Int64:
        return int64_t{0};
    case SmfAttributeType::Float:
        return 0.f;
    case SmfAttributeType::Double:
        return 0.0;
    case SmfAttributeType::Vec2:
        return SmfVec2{};
    case SmfAttributeType::Vec3:
        return SmfVec3{};
    case SmfAttributeType::Vec4:
    case SmfAttributeType::ColorRGBA:
        return SmfVec4{};
    case SmfAttributeType::Quaternion:
        return SmfQuaternion{};
    case SmfAttributeType::Matrix4: {
        SmfMatrix4 m{};
        for (int r = 0; r < 4; ++r) {
            m.m[static_cast<size_t>(r)][r] = 1.f;
        }
        return m;
    }
    case SmfAttributeType::String:
        return std::string{};
    case SmfAttributeType::AssetHash:
    case SmfAttributeType::ArzachelSeed:
        return uint64_t{0};
    case SmfAttributeType::BlobOpaque:
    case SmfAttributeType::SkeletonPose:
        return std::vector<std::byte>{};
    case SmfAttributeType::ElementRef:
        return SMF_INVALID_INDEX;
    case SmfAttributeType::EasingType:
    case SmfAttributeType::TransitionBlendMode:
        return uint8_t{0};
    default:
        return std::monostate{};
    }
}

inline SmfEntity SmfMakeEntity(std::string name, std::string className) {
    SmfEntity e;
    e.Name = std::move(name);
    e.ClassName = std::move(className);
    return e;
}

} // namespace Solstice::Smf
