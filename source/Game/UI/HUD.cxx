#include "UI/HUD.hxx"
#include "Gameplay/Health.hxx"
#include "Gameplay/Inventory.hxx"
#include "Gameplay/Stamina.hxx"
#include "FPS/Weapon.hxx"
#include "FPS/WeaponSwitcher.hxx"
#include "../../Physics/Dynamics/RigidBody.hxx"
#include "../../Core/Debug/Debug.hxx"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace Solstice::Game {

HUD::HUD() {
}

void HUD::Render(ECS::Registry& Registry, const Render::Camera& Camera, int ScreenWidth, int ScreenHeight) {
    // Safety check: ensure ImGui is initialized and in a valid frame
    if (!ImGui::GetCurrentContext()) {
        return;
    }

    // Find player entity (assuming first entity with Health component is player)
    ECS::EntityId playerEntity = 0;
    Registry.ForEach<Health>([&](ECS::EntityId entity, Health& health) {
        if (playerEntity == 0) {
            playerEntity = entity;
        }
    });

    if (playerEntity == 0) return;

    // Safety check: ensure screen dimensions are valid
    if (ScreenWidth <= 0 || ScreenHeight <= 0) {
        return;
    }

    // Render health bar
    if (m_HealthBarVisible) {
        RenderHealthBar(Registry, playerEntity, Math::Vec2(20.0f, ScreenHeight - 60.0f), Math::Vec2(200.0f, 30.0f));
    }

    // Render inventory
    if (m_InventoryVisible) {
        RenderInventory(Registry, playerEntity, Math::Vec2(ScreenWidth * 0.5f - 200.0f, ScreenHeight * 0.5f - 200.0f),
                       Math::Vec2(400.0f, 400.0f));
    }

    // Render minimap
    if (m_MinimapVisible) {
        RenderMinimap(Registry, Camera, Math::Vec2(ScreenWidth - 170.0f, 20.0f), Math::Vec2(150.0f, 150.0f));
    }

    // Render crosshair
    if (m_CrosshairVisible) {
        RenderCrosshair(Math::Vec2(ScreenWidth * 0.5f, ScreenHeight * 0.5f));
    }

    // Render enhanced HUD elements
    if (m_StaminaBarVisible) {
        RenderStaminaBar(Registry, playerEntity, Math::Vec2(20.0f, ScreenHeight - 100.0f), Math::Vec2(200.0f, 25.0f));
    }

    if (m_SpeedIndicatorVisible) {
        RenderSpeedIndicator(Registry, playerEntity, Math::Vec2(20.0f, ScreenHeight - 130.0f));
    }

    if (m_WeaponDisplayVisible) {
        // Half-Life style: weapon display includes ammo, positioned bottom-right
        RenderEquippedWeapon(Registry, playerEntity, Math::Vec2(ScreenWidth - 220.0f, ScreenHeight - 70.0f));
    }

    // Update and render damage indicators
    UpdateDamageIndicators(0.016f, Camera, ScreenWidth, ScreenHeight);
}

void HUD::RenderHealthBar(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec2& Position, const Math::Vec2& Size) {
    // Half-Life style health bar: direct rendering to avoid window artifacts
    float currentHealth = 100.0f;
    float maxHealth = 100.0f;
    float armor = 0.0f;

    if (Registry.Has<Health>(Entity)) {
        auto& health = Registry.Get<Health>(Entity);
        currentHealth = health.CurrentHealth;
        maxHealth = health.MaxHealth;
        armor = health.Armor;
    }

    float healthPercent = maxHealth > 0.0f ? currentHealth / maxHealth : 0.0f;
    healthPercent = std::max(0.0f, std::min(1.0f, healthPercent));

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    // Bar dimensions
    float padding = 4.0f;
    ImVec2 barMin = ImVec2(Position.x + padding, Position.y + padding);
    ImVec2 barMax = ImVec2(Position.x + Size.x - padding, Position.y + Size.y - padding);
    float barHeight = barMax.y - barMin.y;
    float barWidth = barMax.x - barMin.x;

    // Background (dark gray)
    drawList->AddRectFilled(barMin, barMax, IM_COL32(40, 40, 40, 255), 2.0f);

    // Health bar fill (red, with slight gradient effect)
    float healthWidth = barWidth * healthPercent;
    if (healthWidth > 0.0f) {
        ImVec2 healthMax = ImVec2(barMin.x + healthWidth, barMax.y);
        // Main health color - bright red when high, darker when low
        ImU32 healthColor = healthPercent > 0.3f
            ? IM_COL32(200, 50, 50, 255)
            : IM_COL32(150, 30, 30, 255);
        drawList->AddRectFilled(barMin, healthMax, healthColor, 2.0f);

        // Highlight on top of health bar
        if (healthPercent > 0.1f) {
            ImVec2 highlightMax = ImVec2(barMin.x + healthWidth, barMin.y + barHeight * 0.3f);
            drawList->AddRectFilled(barMin, highlightMax, IM_COL32(255, 80, 80, 180), 2.0f);
        }
    }

    // Armor bar (if any) - shown as a separate bar above health
    if (armor > 0.0f) {
        float armorPercent = std::min(armor / 100.0f, 1.0f);
        float armorWidth = barWidth * armorPercent;
        float armorBarHeight = barHeight * 0.3f;
        ImVec2 armorMin = ImVec2(barMin.x, barMin.y);
        ImVec2 armorMax = ImVec2(barMin.x + armorWidth, barMin.y + armorBarHeight);
        drawList->AddRectFilled(armorMin, armorMax, IM_COL32(100, 150, 255, 255), 2.0f);
    }

    // Border (white outline)
    drawList->AddRect(barMin, barMax, IM_COL32(255, 255, 255, 255), 2.0f, 0, 1.5f);

    // Text overlay - health value
    std::stringstream ss;
    ss << std::fixed << std::setprecision(0) << currentHealth;
    if (armor > 0.0f) {
        ss << " | " << static_cast<int>(armor);
    }

    ImVec2 textSize = ImGui::CalcTextSize(ss.str().c_str());
    ImVec2 textPos = ImVec2(
        barMin.x + (barWidth - textSize.x) * 0.5f,
        barMin.y + (barHeight - textSize.y) * 0.5f
    );

    // Text shadow for readability
    drawList->AddText(ImVec2(textPos.x + 1, textPos.y + 1), IM_COL32(0, 0, 0, 200), ss.str().c_str());
    // Text
    drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), ss.str().c_str());
}

void HUD::RenderInventory(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec2& Position, const Math::Vec2& Size) {
    (void)Registry;
    (void)Entity;
    ImGui::SetNextWindowPos(ImVec2(Position.x, Position.y));
    ImGui::SetNextWindowSize(ImVec2(Size.x, Size.y));
    ImGui::Begin("Inventory", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    // Placeholder inventory rendering
    ImGui::Text("Inventory");
    ImGui::Separator();
    ImGui::Text("Item 1");
    ImGui::Text("Item 2");
    ImGui::Text("Item 3");

    ImGui::End();
}

void HUD::RenderMinimap(ECS::Registry& Registry, const Render::Camera& Camera,
                       const Math::Vec2& Position, const Math::Vec2& Size) {
    ImGui::SetNextWindowPos(ImVec2(Position.x, Position.y));
    ImGui::SetNextWindowSize(ImVec2(Size.x, Size.y));
    ImGui::Begin("Minimap", nullptr,
                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoInputs);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 center = ImVec2(Position.x + Size.x * 0.5f, Position.y + Size.y * 0.5f);
    float radius = Size.x * 0.4f;

    // Draw minimap background circle with border
    drawList->AddCircleFilled(center, radius, IM_COL32(20, 20, 30, 200));
    drawList->AddCircle(center, radius, IM_COL32(100, 100, 120, 255), 0, 2.0f);

    // Draw compass directions
    float compassSize = radius * 0.8f;
    drawList->AddLine(ImVec2(center.x, center.y - compassSize), ImVec2(center.x, center.y - compassSize - 8.0f), IM_COL32(150, 150, 150, 200), 1.5f); // N
    drawList->AddLine(ImVec2(center.x + compassSize, center.y), ImVec2(center.x + compassSize + 8.0f, center.y), IM_COL32(150, 150, 150, 200), 1.5f); // E
    drawList->AddLine(ImVec2(center.x, center.y + compassSize), ImVec2(center.x, center.y + compassSize + 8.0f), IM_COL32(150, 150, 150, 200), 1.5f); // S
    drawList->AddLine(ImVec2(center.x - compassSize, center.y), ImVec2(center.x - compassSize - 8.0f, center.y), IM_COL32(150, 150, 150, 200), 1.5f); // W

    // Draw center crosshair
    drawList->AddLine(ImVec2(center.x - 3.0f, center.y), ImVec2(center.x + 3.0f, center.y), IM_COL32(100, 100, 100, 150), 1.0f);
    drawList->AddLine(ImVec2(center.x, center.y - 3.0f), ImVec2(center.x, center.y + 3.0f), IM_COL32(100, 100, 100, 150), 1.0f);

    // Draw player position (green dot with direction indicator)
    drawList->AddCircleFilled(center, 4.0f, IM_COL32(0, 255, 0, 255));
    drawList->AddCircle(center, 4.0f, IM_COL32(0, 200, 0, 255), 0, 1.5f);

    // Draw direction indicator based on camera yaw
    float yawRad = Camera.Yaw * 3.14159265f / 180.0f;
    float dirX = std::cos(yawRad) * 8.0f;
    float dirY = std::sin(yawRad) * 8.0f;
    drawList->AddLine(center, ImVec2(center.x + dirX, center.y - dirY), IM_COL32(0, 255, 0, 200), 2.0f);

    // Draw enemies (placeholder)
    Registry.ForEach<Health>([&](ECS::EntityId entity, Health& health) {
        // In a real implementation, we'd project entity positions to minimap
        // For now, this is a placeholder
    });

    ImGui::End();
}

void HUD::RenderCrosshair(const Math::Vec2& Center, float Size) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    float halfSize = Size * 0.5f;
    float gap = 4.0f; // Gap in center

    // Draw crosshair with gap in center (more visible)
    // Top line
    drawList->AddLine(ImVec2(Center.x, Center.y - halfSize),
                     ImVec2(Center.x, Center.y - gap),
                     IM_COL32(255, 255, 255, 240), 2.0f);
    // Bottom line
    drawList->AddLine(ImVec2(Center.x, Center.y + gap),
                     ImVec2(Center.x, Center.y + halfSize),
                     IM_COL32(255, 255, 255, 240), 2.0f);
    // Left line
    drawList->AddLine(ImVec2(Center.x - halfSize, Center.y),
                     ImVec2(Center.x - gap, Center.y),
                     IM_COL32(255, 255, 255, 240), 2.0f);
    // Right line
    drawList->AddLine(ImVec2(Center.x + gap, Center.y),
                     ImVec2(Center.x + halfSize, Center.y),
                     IM_COL32(255, 255, 255, 240), 2.0f);

    // Center dot
    drawList->AddCircleFilled(ImVec2(Center.x, Center.y), 1.5f, IM_COL32(255, 255, 255, 200));
}

void HUD::RenderAmmo(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec2& Position) {
    // Ammo is now displayed in the weapon display, so this can be simplified or removed
    // Keeping it for backwards compatibility but making it minimal
    (void)Registry;
    (void)Entity;
    (void)Position;
    // Ammo is now part of RenderEquippedWeapon
}

void HUD::RenderStaminaBar(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec2& Position, const Math::Vec2& Size) {
    // Snap window position to pixel boundaries to prevent sub-pixel rendering artifacts
    ImVec2 snappedPos = ImVec2(roundf(Position.x), roundf(Position.y));
    ImGui::SetNextWindowPos(snappedPos);
    ImGui::SetNextWindowSize(ImVec2(Size.x, Size.y));
    // Disable rounding, borders, and padding to prevent visual artifacts
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

    bool windowOpen = ImGui::Begin("StaminaBar", nullptr,
                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoInputs);

    if (!windowOpen) {
        // Window failed to open - pop vars before returning
        ImGui::PopStyleVar(4);
        return;
    }

    float staminaPercent = 1.0f;
    float currentStamina = 100.0f;
    float maxStamina = 100.0f;

    if (Registry.Has<Stamina>(Entity)) {
        auto& stamina = Registry.Get<Stamina>(Entity);
        currentStamina = stamina.CurrentStamina;
        maxStamina = stamina.MaxStamina;
        staminaPercent = maxStamina > 0.0f ? currentStamina / maxStamina : 0.0f;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();

    // Temporarily disable anti-aliasing for this draw list only
    ImDrawListFlags oldFlags = drawList->Flags;
    drawList->Flags &= ~ImDrawListFlags_AntiAliasedFill;
    drawList->Flags &= ~ImDrawListFlags_AntiAliasedLines;

    // Use exact integer coordinates (no +0.5 offset) for true pixel-perfect rendering
    ImVec2 minInt = ImVec2(floorf(windowPos.x), floorf(windowPos.y));
    ImVec2 maxInt = ImVec2(floorf(windowPos.x + windowSize.x), floorf(windowPos.y + windowSize.y));

    // Background - use integer coordinates
    drawList->AddRectFilled(minInt, maxInt, IM_COL32(50, 50, 50, 255), 0.0f);

    // Stamina bar - use integer coordinates for pixel-perfect rendering
    float staminaWidth = windowSize.x * staminaPercent;
    ImVec2 staminaMax = ImVec2(floorf(minInt.x + staminaWidth), maxInt.y);
    ImU32 color = staminaPercent > 0.3f ? IM_COL32(50, 200, 50, 255) : IM_COL32(200, 200, 50, 255);
    drawList->AddRectFilled(minInt, staminaMax, color, 0.0f);

    // Border - draw as individual 1-pixel lines with exact integer coordinates
    // Offset by 0.5 to align with pixel edges for crisp 1-pixel borders
    drawList->AddLine(ImVec2(minInt.x + 0.5f, minInt.y + 0.5f), ImVec2(maxInt.x - 0.5f, minInt.y + 0.5f), IM_COL32(255, 255, 255, 255), 1.0f); // Top
    drawList->AddLine(ImVec2(maxInt.x - 0.5f, minInt.y + 0.5f), ImVec2(maxInt.x - 0.5f, maxInt.y - 0.5f), IM_COL32(255, 255, 255, 255), 1.0f); // Right
    drawList->AddLine(ImVec2(maxInt.x - 0.5f, maxInt.y - 0.5f), ImVec2(minInt.x + 0.5f, maxInt.y - 0.5f), IM_COL32(255, 255, 255, 255), 1.0f); // Bottom
    drawList->AddLine(ImVec2(minInt.x + 0.5f, maxInt.y - 0.5f), ImVec2(minInt.x + 0.5f, minInt.y + 0.5f), IM_COL32(255, 255, 255, 255), 1.0f); // Left

    // Restore draw list flags
    drawList->Flags = oldFlags;

    // Text
    std::stringstream ss;
    ss << std::fixed << std::setprecision(0) << currentStamina << " / " << maxStamina;
    ImVec2 textSize = ImGui::CalcTextSize(ss.str().c_str());
    ImGui::SetCursorPos(ImVec2((Size.x - textSize.x) * 0.5f, (Size.y - textSize.y) * 0.5f));
    ImGui::Text(ss.str().c_str());

    ImGui::End();
    ImGui::PopStyleVar(4); // Pop all four style vars (padding, rounding, border, frame rounding)
}

void HUD::RenderSpeedIndicator(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec2& Position) {
    // Direct rendering to avoid window artifacts
    float speed = 0.0f;

    if (Registry.Has<Physics::RigidBody>(Entity)) {
        auto& rb = Registry.Get<Physics::RigidBody>(Entity);
        Math::Vec3 horizontalVel = rb.Velocity;
        horizontalVel.y = 0.0f;
        speed = horizontalVel.Magnitude();
    }

    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << speed << " m/s";
    std::string speedText = ss.str();

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImVec2 textSize = ImGui::CalcTextSize(speedText.c_str());
    ImVec2 textPos(Position.x, Position.y);

    // Text shadow for readability
    drawList->AddText(ImVec2(textPos.x + 1, textPos.y + 1), IM_COL32(0, 0, 0, 200), speedText.c_str());
    // Text
    drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), speedText.c_str());
}

void HUD::RenderEquippedWeapon(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec2& Position) {
    // Half-Life style weapon display: weapon name and ammo combined
    ImGui::SetNextWindowPos(ImVec2(Position.x, Position.y));
    ImGui::SetNextWindowSize(ImVec2(200.0f, 60.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

    bool windowOpen = ImGui::Begin("Weapon", nullptr,
                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoInputs);

    if (!windowOpen) {
        ImGui::PopStyleVar(4);
        return;
    }

    std::string weaponName = "None";
    int currentAmmo = 0;
    int maxAmmo = 0;
    int reserveAmmo = 0;
    bool isReloading = false;

    if (Registry.Has<Weapon>(Entity)) {
        auto& weapon = Registry.Get<Weapon>(Entity);
        weaponName = weapon.Name.empty() ? "Weapon" : weapon.Name;
        currentAmmo = weapon.CurrentAmmo;
        maxAmmo = weapon.MaxAmmo;
        reserveAmmo = weapon.ReserveAmmo;
        isReloading = weapon.IsReloading;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();

    // Background
    ImVec2 bgMin = windowPos;
    ImVec2 bgMax = ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y);
    drawList->AddRectFilled(bgMin, bgMax, IM_COL32(30, 30, 30, 220), 2.0f);

    // Border
    drawList->AddRect(bgMin, bgMax, IM_COL32(200, 200, 200, 255), 2.0f, 0, 1.5f);

    // Weapon name (top)
    ImVec2 namePos = ImVec2(windowPos.x + 6, windowPos.y + 4);
    drawList->AddText(namePos, IM_COL32(255, 255, 255, 255), weaponName.c_str());

    // Ammo display (bottom, larger)
    std::stringstream ammoSS;
    if (maxAmmo > 0) {
        ammoSS << currentAmmo << " / " << maxAmmo;
        if (reserveAmmo > 0) {
            ammoSS << " (" << reserveAmmo << ")";
        }
    } else {
        ammoSS << "--";
    }

    if (isReloading) {
        ammoSS << " [RELOADING]";
    }

    std::string ammoText = ammoSS.str();
    ImVec2 ammoTextSize = ImGui::CalcTextSize(ammoText.c_str());
    ImVec2 ammoPos = ImVec2(
        windowPos.x + 6,
        windowPos.y + windowSize.y - ammoTextSize.y - 4
    );

    // Ammo text color (yellow when reloading)
    ImU32 ammoColor = isReloading
        ? IM_COL32(255, 255, 0, 255)
        : IM_COL32(255, 255, 255, 255);

    // Text shadow
    drawList->AddText(ImVec2(ammoPos.x + 1, ammoPos.y + 1), IM_COL32(0, 0, 0, 200), ammoText.c_str());
    // Ammo text
    drawList->AddText(ammoPos, ammoColor, ammoText.c_str());

    ImGui::End();
    ImGui::PopStyleVar(4);
}

void HUD::RenderHealthDisplay(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec2& Position) {
    // Health display is now integrated into the health bar, so this is no longer needed
    // Keeping for backwards compatibility but making it a no-op
    (void)Registry;
    (void)Entity;
    (void)Position;
}

void HUD::RenderObjective(const std::string& Text, const Math::Vec2& Position) {
    ImGui::SetNextWindowPos(ImVec2(Position.x, Position.y));
    ImGui::Begin("Objective", nullptr,
                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoInputs);

    ImGui::Text(Text.c_str());

    ImGui::End();
}

void HUD::AddDamageIndicator(const Math::Vec3& WorldPos, float Damage, bool IsCritical) {
    DamageIndicator indicator;
    indicator.WorldPosition = WorldPos;
    indicator.Damage = Damage;
    indicator.IsCritical = IsCritical;
    m_DamageIndicators.push_back(indicator);
}

void HUD::UpdateDamageIndicators(float DeltaTime, const Render::Camera& Camera, int ScreenWidth, int ScreenHeight) {
    auto it = m_DamageIndicators.begin();
    while (it != m_DamageIndicators.end()) {
        it->Timer += DeltaTime;

        if (it->Timer >= it->Lifetime) {
            it = m_DamageIndicators.erase(it);
        } else {
            // Render damage indicator
            Math::Vec2 screenPos = WorldToScreen(it->WorldPosition, Camera, ScreenWidth, ScreenHeight);
            float alpha = 1.0f - (it->Timer / it->Lifetime);

            ImDrawList* drawList = ImGui::GetBackgroundDrawList();
            std::string damageText = std::to_string(static_cast<int>(it->Damage));
            ImVec4 color = it->IsCritical ? ImVec4(1.0f, 0.0f, 0.0f, alpha) : ImVec4(1.0f, 1.0f, 1.0f, alpha);

            drawList->AddText(ImVec2(screenPos.x, screenPos.y - it->Timer * 50.0f),
                             IM_COL32(static_cast<int>(color.x * 255), static_cast<int>(color.y * 255),
                                     static_cast<int>(color.z * 255), static_cast<int>(color.w * 255)),
                             damageText.c_str());

            ++it;
        }
    }
}

Math::Vec2 HUD::WorldToScreen(const Math::Vec3& WorldPos, const Render::Camera& Camera,
                             int ScreenWidth, int ScreenHeight) const {
    // Placeholder - would use view/projection matrices
    // For now, return center of screen
    return Math::Vec2(ScreenWidth * 0.5f, ScreenHeight * 0.5f);
}

} // namespace Solstice::Game
