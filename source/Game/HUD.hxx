#pragma once

#include "../Solstice.hxx"
#include "../Entity/Registry.hxx"
#include <Render/Scene/Camera.hxx>
#include "../Math/Vector.hxx"
#include <string>
#include <vector>

// Forward declare ImGui types
struct ImVec2;
struct ImVec4;

namespace Solstice::Game {

// HUD element types
enum class HUDElementType {
    HealthBar,
    Inventory,
    Minimap,
    Crosshair,
    Ammo,
    Objective,
    DamageIndicator,
    COUNT
};

// Damage indicator
struct DamageIndicator {
    Math::Vec3 WorldPosition;
    float Damage;
    float Lifetime{2.0f};
    float Timer{0.0f};
    bool IsCritical{false};
};

// HUD system
class SOLSTICE_API HUD {
public:
    HUD();
    ~HUD() = default;

    // Render HUD (call each frame)
    void Render(ECS::Registry& Registry, const Render::Camera& Camera, int ScreenWidth, int ScreenHeight);

    // Health bar
    void RenderHealthBar(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec2& Position, const Math::Vec2& Size);

    // Inventory display
    void RenderInventory(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec2& Position, const Math::Vec2& Size);

    // Minimap
    void RenderMinimap(ECS::Registry& Registry, const Render::Camera& Camera,
                      const Math::Vec2& Position, const Math::Vec2& Size);

    // Crosshair
    void RenderCrosshair(const Math::Vec2& Center, float Size = 20.0f);

    // Ammo/weapon display
    void RenderAmmo(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec2& Position);

    // Enhanced HUD elements
    void RenderStaminaBar(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec2& Position, const Math::Vec2& Size);
    void RenderSpeedIndicator(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec2& Position);
    void RenderEquippedWeapon(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec2& Position);
    void RenderHealthDisplay(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec2& Position); // Enhanced with numeric values

    // Objective display
    void RenderObjective(const std::string& Text, const Math::Vec2& Position);

    // Damage indicators
    void AddDamageIndicator(const Math::Vec3& WorldPos, float Damage, bool IsCritical = false);
    void UpdateDamageIndicators(float DeltaTime, const Render::Camera& Camera, int ScreenWidth, int ScreenHeight);

    // Configuration
    void SetHealthBarVisible(bool Visible) { m_HealthBarVisible = Visible; }
    void SetInventoryVisible(bool Visible) { m_InventoryVisible = Visible; }
    void SetMinimapVisible(bool Visible) { m_MinimapVisible = Visible; }
    void SetCrosshairVisible(bool Visible) { m_CrosshairVisible = Visible; }
    void SetStaminaBarVisible(bool Visible) { m_StaminaBarVisible = Visible; }
    void SetSpeedIndicatorVisible(bool Visible) { m_SpeedIndicatorVisible = Visible; }
    void SetWeaponDisplayVisible(bool Visible) { m_WeaponDisplayVisible = Visible; }

private:
    bool m_HealthBarVisible{true};
    bool m_InventoryVisible{false};
    bool m_MinimapVisible{true};
    bool m_CrosshairVisible{true};
    bool m_StaminaBarVisible{true};
    bool m_SpeedIndicatorVisible{true};
    bool m_WeaponDisplayVisible{true};

    std::vector<DamageIndicator> m_DamageIndicators;

    // Helper to convert world position to screen position
    Math::Vec2 WorldToScreen(const Math::Vec3& WorldPos, const Render::Camera& Camera,
                             int ScreenWidth, int ScreenHeight) const;
};

} // namespace Solstice::Game
