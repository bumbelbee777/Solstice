#pragma once

#include <Math/Matrix.hxx>
#include <Math/Vector.hxx>

namespace Solstice::Core::Audio {

class PortalAudio {
public:
    void SetPortalTransform(const Math::Matrix4& Transform, bool Enabled);
    void ClearPortalTransform();

    void TransformRay(Math::Vec3& Origin, Math::Vec3& Direction, float& Energy, float& Distance) const;
    bool IsListenerInPortal(const Math::Vec3& ListenerPos) const;
    float GetPortalReverbModifier() const { return m_ReverbModifier; }

private:
    Math::Matrix4 m_LocalToOther{};
    bool m_Active{false};
    float m_ReverbModifier{1.0f};
};

} // namespace Solstice::Core::Audio
