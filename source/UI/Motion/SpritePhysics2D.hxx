#pragma once

#include "../../Solstice.hxx"
#include <imgui.h>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace Solstice::UI {

class Sprite;

/**
 * Lightweight 2D AABB physics for UI sprites (screen-space, no engine Physics dependency).
 */
class SOLSTICE_API SpritePhysicsWorld {
public:
    SpritePhysicsWorld();
    ~SpritePhysicsWorld() = default;

    void SetGravity(const ImVec2& g) { m_Gravity = g; }
    ImVec2 GetGravity() const { return m_Gravity; }

    void SetBounds(const ImVec2& min, const ImVec2& max) {
        m_BoundsMin = min;
        m_BoundsMax = max;
        m_HasBounds = true;
    }
    void ClearBounds() { m_HasBounds = false; }

    void SetRestitution(float e) { m_Restitution = std::min(1.0f, std::max(0.0f, e)); }
    float GetRestitution() const { return m_Restitution; }

    uint32_t AddAabbBody(const ImVec2& center, const ImVec2& halfExtents, float mass, bool dynamicBody);
    void RemoveBody(uint32_t bodyId);
    bool IsValid(uint32_t bodyId) const;

    void SetBodyCenter(uint32_t id, const ImVec2& c);
    ImVec2 GetBodyCenter(uint32_t id) const;
    void SetBodyVelocity(uint32_t id, const ImVec2& v);
    ImVec2 GetBodyVelocity(uint32_t id) const;

    void Step(float dt);

    void SyncSpriteTopLeft(uint32_t id, Sprite& sprite) const;

    void Clear();

private:
    struct Body {
        uint32_t id{0};
        ImVec2 center{0.0f, 0.0f};
        ImVec2 half{1.0f, 1.0f};
        ImVec2 vel{0.0f, 0.0f};
        float invMass{0.0f};
        bool dynamic{true};
    };

    int FindIndex(uint32_t id) const;
    void Integrate(float dt);
    void SolveBounds();
    void ResolvePair(Body& a, Body& b);

    std::vector<Body> m_Bodies;
    uint32_t m_NextId{1};
    ImVec2 m_Gravity{0.0f, 480.0f};
    ImVec2 m_BoundsMin{0.0f, 0.0f};
    ImVec2 m_BoundsMax{1280.0f, 720.0f};
    bool m_HasBounds{false};
    float m_Restitution{0.12f};
};

} // namespace Solstice::UI
