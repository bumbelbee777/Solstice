#include "PortalDebugDraw.hxx"

namespace Solstice::Render {

PortalDebugDrawBuffer& PortalDebugDrawBuffer::Instance() {
    static PortalDebugDrawBuffer s_Instance;
    return s_Instance;
}

void PortalDebugDrawBuffer::BeginFrame() {
    m_Lines.clear();
}

void PortalDebugDrawBuffer::AddLine(const Math::Vec3& A, const Math::Vec3& B, uint32_t Color) {
    PortalDebugLine L{};
    L.P1 = A;
    L.P2 = B;
    L.Color1 = Color;
    L.Color2 = Color;
    m_Lines.push_back(L);
}

void PortalDebugDrawBuffer::AddLine(const PortalDebugLine& L) {
    m_Lines.push_back(L);
}

} // namespace Solstice::Render
