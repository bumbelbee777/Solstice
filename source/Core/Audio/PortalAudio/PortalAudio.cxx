#include "PortalAudio.hxx"

namespace Solstice::Core::Audio {

void PortalAudio::SetPortalTransform(const Math::Matrix4& Transform, bool Enabled) {
    m_LocalToOther = Transform;
    m_Active = Enabled;
    m_ReverbModifier = Enabled ? 1.15f : 1.0f;
}

void PortalAudio::ClearPortalTransform() {
    m_Active = false;
    m_LocalToOther = Math::Matrix4::Identity();
    m_ReverbModifier = 1.0f;
}

void PortalAudio::TransformRay(Math::Vec3& Origin, Math::Vec3& Direction, float& Energy, float& Distance) const {
    if (!m_Active) {
        return;
    }
    Math::Vec4 oh = m_LocalToOther * Math::Vec4(Origin.x, Origin.y, Origin.z, 1.0f);
    Origin = Math::Vec3(oh.x, oh.y, oh.z);
    Math::Vec4 dh = m_LocalToOther * Math::Vec4(Direction.x, Direction.y, Direction.z, 0.0f);
    Direction = Math::Vec3(dh.x, dh.y, dh.z).Normalized();
    Energy *= 0.92f;
    Distance *= 1.05f;
}

bool PortalAudio::IsListenerInPortal(const Math::Vec3& ListenerPos) const {
    if (!m_Active) {
        return false;
    }
    return ListenerPos.Dot(ListenerPos) < 64.0f;
}

} // namespace Solstice::Core::Audio
