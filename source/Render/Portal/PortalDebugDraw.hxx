#pragma once

#include <Solstice.hxx>
#include <Math/Vector.hxx>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Solstice::Render {

struct PortalDebugLine {
    Math::Vec3 P1{};
    Math::Vec3 P2{};
    uint32_t Color1{0xFFFFFFu};
    uint32_t Color2{0xFFFFFFu};
};

/// Per-frame line list produced by gameplay (ECS PortalSystem); consumed by SoftwareRenderer on VIEW_SCENE.
class SOLSTICE_API PortalDebugDrawBuffer {
public:
    static PortalDebugDrawBuffer& Instance();

    void BeginFrame();

    void AddLine(const Math::Vec3& A, const Math::Vec3& B, uint32_t Color);
    void AddLine(const PortalDebugLine& L);

    const std::vector<PortalDebugLine>& GetLines() const { return m_Lines; }
    bool HasLines() const { return !m_Lines.empty(); }

private:
    PortalDebugDrawBuffer() = default;

    std::vector<PortalDebugLine> m_Lines{};
};

inline uint32_t PackPortalLineColor(float R, float G, float B) {
    const auto C = [](float x) -> uint32_t {
        const float t = std::round(std::clamp(x, 0.0f, 1.0f) * 255.0f);
        return static_cast<uint32_t>(t);
    };
    return (C(R) << 16) | (C(G) << 8) | C(B);
}

} // namespace Solstice::Render
