#include <UI/Motion/SpritePhysics2D.hxx>
#include <UI/Motion/Sprite.hxx>
#include <algorithm>
#include <cmath>

namespace Solstice::UI {

namespace {

float Dot2(const ImVec2& a, const ImVec2& b) {
    return a.x * b.x + a.y * b.y;
}

} // namespace

SpritePhysicsWorld::SpritePhysicsWorld() = default;

int SpritePhysicsWorld::FindIndex(uint32_t id) const {
    for (size_t i = 0; i < m_Bodies.size(); ++i) {
        if (m_Bodies[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

uint32_t SpritePhysicsWorld::AddAabbBody(const ImVec2& center, const ImVec2& halfExtents, float mass, bool dynamicBody) {
    Body b;
    b.id = m_NextId++;
    b.center = center;
    b.half = halfExtents;
    b.dynamic = dynamicBody;
    if (dynamicBody && mass > 0.0f) {
        b.invMass = 1.0f / mass;
    } else {
        b.invMass = 0.0f;
        b.dynamic = false;
    }
    m_Bodies.push_back(b);
    return b.id;
}

void SpritePhysicsWorld::RemoveBody(uint32_t bodyId) {
    const int idx = FindIndex(bodyId);
    if (idx < 0) {
        return;
    }
    m_Bodies.erase(m_Bodies.begin() + idx);
}

bool SpritePhysicsWorld::IsValid(uint32_t bodyId) const {
    return FindIndex(bodyId) >= 0;
}

void SpritePhysicsWorld::SetBodyCenter(uint32_t id, const ImVec2& c) {
    const int idx = FindIndex(id);
    if (idx >= 0) {
        m_Bodies[static_cast<size_t>(idx)].center = c;
    }
}

ImVec2 SpritePhysicsWorld::GetBodyCenter(uint32_t id) const {
    const int idx = FindIndex(id);
    if (idx >= 0) {
        return m_Bodies[static_cast<size_t>(idx)].center;
    }
    return ImVec2(0.0f, 0.0f);
}

void SpritePhysicsWorld::SetBodyVelocity(uint32_t id, const ImVec2& v) {
    const int idx = FindIndex(id);
    if (idx >= 0 && m_Bodies[static_cast<size_t>(idx)].dynamic) {
        m_Bodies[static_cast<size_t>(idx)].vel = v;
    }
}

ImVec2 SpritePhysicsWorld::GetBodyVelocity(uint32_t id) const {
    const int idx = FindIndex(id);
    if (idx >= 0) {
        return m_Bodies[static_cast<size_t>(idx)].vel;
    }
    return ImVec2(0.0f, 0.0f);
}

void SpritePhysicsWorld::Integrate(float dt) {
    for (Body& b : m_Bodies) {
        if (!b.dynamic) {
            continue;
        }
        b.vel.x += m_Gravity.x * dt;
        b.vel.y += m_Gravity.y * dt;
        b.center.x += b.vel.x * dt;
        b.center.y += b.vel.y * dt;
    }
}

void SpritePhysicsWorld::SolveBounds() {
    if (!m_HasBounds) {
        return;
    }
    for (Body& b : m_Bodies) {
        if (!b.dynamic) {
            continue;
        }
        float minX = b.center.x - b.half.x;
        float maxX = b.center.x + b.half.x;
        float minY = b.center.y - b.half.y;
        float maxY = b.center.y + b.half.y;
        if (minX < m_BoundsMin.x) {
            b.center.x = m_BoundsMin.x + b.half.x;
            b.vel.x *= -m_Restitution;
        } else if (maxX > m_BoundsMax.x) {
            b.center.x = m_BoundsMax.x - b.half.x;
            b.vel.x *= -m_Restitution;
        }
        if (minY < m_BoundsMin.y) {
            b.center.y = m_BoundsMin.y + b.half.y;
            b.vel.y *= -m_Restitution;
        } else if (maxY > m_BoundsMax.y) {
            b.center.y = m_BoundsMax.y - b.half.y;
            b.vel.y *= -m_Restitution;
        }
    }
}

void SpritePhysicsWorld::ResolvePair(Body& a, Body& b) {
    if (!a.dynamic && !b.dynamic) {
        return;
    }
    const float dx = b.center.x - a.center.x;
    const float dy = b.center.y - a.center.y;
    const float ax = (a.half.x + b.half.x) - std::fabs(dx);
    if (ax <= 0.0f) {
        return;
    }
    const float ay = (a.half.y + b.half.y) - std::fabs(dy);
    if (ay <= 0.0f) {
        return;
    }
    ImVec2 normal;
    float depth;
    if (ax < ay) {
        depth = ax;
        normal = ImVec2(dx < 0.0f ? -1.0f : 1.0f, 0.0f);
    } else {
        depth = ay;
        normal = ImVec2(0.0f, dy < 0.0f ? -1.0f : 1.0f);
    }
    const float invMassSum = a.invMass + b.invMass;
    if (invMassSum <= 0.0f) {
        return;
    }
    constexpr float percent = 0.85f;
    constexpr float slop = 0.05f;
    const float corrMag = std::max(depth - slop, 0.0f) / invMassSum * percent;
    const ImVec2 corr(normal.x * corrMag, normal.y * corrMag);
    a.center.x -= corr.x * a.invMass;
    a.center.y -= corr.y * a.invMass;
    b.center.x += corr.x * b.invMass;
    b.center.y += corr.y * b.invMass;

    const ImVec2 rel(b.vel.x - a.vel.x, b.vel.y - a.vel.y);
    const float vn = Dot2(rel, normal);
    if (vn >= 0.0f) {
        return;
    }
    const float j = -(1.0f + m_Restitution) * vn / invMassSum;
    const ImVec2 impulse(normal.x * j, normal.y * j);
    if (a.dynamic) {
        a.vel.x -= impulse.x * a.invMass;
        a.vel.y -= impulse.y * a.invMass;
    }
    if (b.dynamic) {
        b.vel.x += impulse.x * b.invMass;
        b.vel.y += impulse.y * b.invMass;
    }
}

void SpritePhysicsWorld::Step(float dt) {
    Integrate(dt);
    SolveBounds();
    constexpr int kIterations = 4;
    for (int iter = 0; iter < kIterations; ++iter) {
        for (size_t i = 0; i < m_Bodies.size(); ++i) {
            for (size_t j = i + 1; j < m_Bodies.size(); ++j) {
                ResolvePair(m_Bodies[i], m_Bodies[j]);
            }
        }
        SolveBounds();
    }
}

void SpritePhysicsWorld::SyncSpriteTopLeft(uint32_t id, Sprite& sprite) const {
    const int idx = FindIndex(id);
    if (idx < 0) {
        return;
    }
    const Body& b = m_Bodies[static_cast<size_t>(idx)];
    sprite.SetPosition(ImVec2(b.center.x - b.half.x, b.center.y - b.half.y));
}

void SpritePhysicsWorld::Clear() {
    m_Bodies.clear();
    m_NextId = 1;
}

} // namespace Solstice::UI
