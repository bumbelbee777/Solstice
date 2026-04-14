#include "WeaponSwitcher.hxx"
#include "../Gameplay/Inventory.hxx"
#include "../../Core/Debug/Debug.hxx"
#include "../../UI/Core/UISystem.hxx"
#include <imgui.h>
#include <algorithm>

namespace Solstice::Game {

void WeaponSwitcherSystem::Update(ECS::Registry& Registry, float DeltaTime) {
    Registry.ForEach<WeaponSwitcher>([&](ECS::EntityId entity, WeaponSwitcher& switcher) {
        if (switcher.IsSwitching) {
            switcher.SwitchTimer += DeltaTime;
            if (switcher.SwitchTimer >= switcher.SwitchTime) {
                switcher.IsSwitching = false;
                switcher.SwitchTimer = 0.0f;
            }
        }
    });
}

bool WeaponSwitcherSystem::SwitchToWeapon(ECS::Registry& Registry, ECS::EntityId Entity, int WeaponIndex) {
    if (!Registry.Has<WeaponSwitcher>(Entity)) return false;
    if (!CanSwitch(Registry, Entity)) return false;

    WeaponSwitcher& switcher = Registry.Get<WeaponSwitcher>(Entity);

    if (WeaponIndex < 0 || WeaponIndex >= static_cast<int>(switcher.WeaponInventory.size())) {
        return false;
    }

    PerformSwitch(Registry, Entity, WeaponIndex);
    return true;
}

bool WeaponSwitcherSystem::SwitchToNextWeapon(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<WeaponSwitcher>(Entity)) return false;

    WeaponSwitcher& switcher = Registry.Get<WeaponSwitcher>(Entity);
    if (switcher.WeaponInventory.empty()) return false;

    int nextIndex = (switcher.CurrentWeaponIndex + 1) % static_cast<int>(switcher.WeaponInventory.size());
    return SwitchToWeapon(Registry, Entity, nextIndex);
}

bool WeaponSwitcherSystem::SwitchToPreviousWeapon(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<WeaponSwitcher>(Entity)) return false;

    WeaponSwitcher& switcher = Registry.Get<WeaponSwitcher>(Entity);
    if (switcher.WeaponInventory.empty()) return false;

    int prevIndex = (switcher.CurrentWeaponIndex - 1 + static_cast<int>(switcher.WeaponInventory.size()))
                    % static_cast<int>(switcher.WeaponInventory.size());
    return SwitchToWeapon(Registry, Entity, prevIndex);
}

bool WeaponSwitcherSystem::AddWeapon(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t WeaponItemID) {
    if (!Registry.Has<WeaponSwitcher>(Entity)) return false;
    if (!Registry.Has<Inventory>(Entity)) return false;

    WeaponSwitcher& switcher = Registry.Get<WeaponSwitcher>(Entity);
    Inventory& inventory = Registry.Get<Inventory>(Entity);

    // Check if weapon exists in inventory
    Item* item = InventorySystem::GetItem(Registry, Entity, WeaponItemID);
    if (!item || item->Type != ItemType::Weapon) {
        return false;
    }

    // Check if already in weapon inventory
    auto it = std::find(switcher.WeaponInventory.begin(), switcher.WeaponInventory.end(), WeaponItemID);
    if (it != switcher.WeaponInventory.end()) {
        return false; // Already have this weapon
    }

    switcher.WeaponInventory.push_back(WeaponItemID);

    // If no weapon equipped, equip this one
    if (switcher.CurrentWeaponIndex == -1) {
        switcher.CurrentWeaponIndex = static_cast<int>(switcher.WeaponInventory.size()) - 1;
    }

    return true;
}

bool WeaponSwitcherSystem::RemoveWeapon(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t WeaponItemID) {
    if (!Registry.Has<WeaponSwitcher>(Entity)) return false;

    WeaponSwitcher& switcher = Registry.Get<WeaponSwitcher>(Entity);

    auto it = std::find(switcher.WeaponInventory.begin(), switcher.WeaponInventory.end(), WeaponItemID);
    if (it == switcher.WeaponInventory.end()) {
        return false;
    }

    int index = static_cast<int>(std::distance(switcher.WeaponInventory.begin(), it));

    // If removing current weapon, switch to another
    if (switcher.CurrentWeaponIndex == index) {
        if (switcher.WeaponInventory.size() > 1) {
            // Switch to next weapon, or previous if at end
            if (index < static_cast<int>(switcher.WeaponInventory.size()) - 1) {
                switcher.CurrentWeaponIndex = index;
            } else {
                switcher.CurrentWeaponIndex = index - 1;
            }
        } else {
            switcher.CurrentWeaponIndex = -1;
        }
    } else if (switcher.CurrentWeaponIndex > index) {
        switcher.CurrentWeaponIndex--;
    }

    switcher.WeaponInventory.erase(it);
    return true;
}

Weapon* WeaponSwitcherSystem::GetCurrentWeapon(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<WeaponSwitcher>(Entity)) return nullptr;

    WeaponSwitcher& switcher = Registry.Get<WeaponSwitcher>(Entity);
    if (switcher.CurrentWeaponIndex < 0 ||
        switcher.CurrentWeaponIndex >= static_cast<int>(switcher.WeaponInventory.size())) {
        return nullptr;
    }

    // Get weapon from inventory item
    uint32_t weaponItemID = switcher.WeaponInventory[switcher.CurrentWeaponIndex];
    Item* item = InventorySystem::GetItem(Registry, Entity, weaponItemID);
    if (!item) return nullptr;

    // Weapon component would be stored on the entity or in the item properties
    // For now, return nullptr (would need proper integration)
    return nullptr;
}

int WeaponSwitcherSystem::GetCurrentWeaponIndex(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<WeaponSwitcher>(Entity)) return -1;
    return Registry.Get<WeaponSwitcher>(Entity).CurrentWeaponIndex;
}

void WeaponSwitcherSystem::HandleInput(ECS::Registry& Registry, ECS::EntityId Entity, int KeyCode, float ScrollDelta) {
    if (!Registry.Has<WeaponSwitcher>(Entity)) return;

    WeaponSwitcher& switcher = Registry.Get<WeaponSwitcher>(Entity);

    // Check quick switch keys (1-9)
    for (size_t i = 0; i < switcher.QuickSwitchKeys.size() && i < switcher.WeaponInventory.size(); ++i) {
        if (KeyCode == switcher.QuickSwitchKeys[i]) {
            SwitchToWeapon(Registry, Entity, static_cast<int>(i));
            return;
        }
    }

    // Scroll wheel
    if (ScrollDelta > 0.0f) {
        SwitchToNextWeapon(Registry, Entity);
    } else if (ScrollDelta < 0.0f) {
        SwitchToPreviousWeapon(Registry, Entity);
    }
}

void WeaponSwitcherSystem::RenderWeaponWheel(ECS::Registry& Registry, ECS::EntityId Entity, int ScreenWidth, int ScreenHeight) {
    if (!Registry.Has<WeaponSwitcher>(Entity)) return;

    WeaponSwitcher& switcher = Registry.Get<WeaponSwitcher>(Entity);
    if (switcher.WeaponInventory.empty()) return;

    // Render circular weapon wheel
    ImVec2 center(static_cast<float>(ScreenWidth) * 0.5f, static_cast<float>(ScreenHeight) * 0.5f);
    float radius = 150.0f;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    // Draw circle background
    drawList->AddCircleFilled(center, radius, ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.7f)));
    drawList->AddCircle(center, radius, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));

    // Draw weapons around circle
    float angleStep = (2.0f * 3.14159f) / static_cast<float>(switcher.WeaponInventory.size());
    for (size_t i = 0; i < switcher.WeaponInventory.size(); ++i) {
        float angle = static_cast<float>(i) * angleStep;
        float x = center.x + std::cos(angle) * (radius * 0.7f);
        float y = center.y + std::sin(angle) * (radius * 0.7f);

        bool isSelected = (static_cast<int>(i) == switcher.CurrentWeaponIndex);
        ImU32 color = isSelected ?
            ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 0.0f, 1.0f)) :
            ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

        drawList->AddCircleFilled(ImVec2(x, y), 20.0f, color);

        // Draw weapon number
        std::string numText = std::to_string(i + 1);
        ImVec2 textSize = ImGui::CalcTextSize(numText.c_str());
        drawList->AddText(ImVec2(x - textSize.x * 0.5f, y - textSize.y * 0.5f),
                         ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 1.0f)),
                         numText.c_str());
    }
}

bool WeaponSwitcherSystem::CanSwitch(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<WeaponSwitcher>(Entity)) return false;

    WeaponSwitcher& switcher = Registry.Get<WeaponSwitcher>(Entity);
    if (switcher.IsSwitching) return false;

    // Check if weapon is reloading
    Weapon* weapon = GetCurrentWeapon(Registry, Entity);
    if (weapon && weapon->IsReloading) return false;

    return true;
}

void WeaponSwitcherSystem::PerformSwitch(ECS::Registry& Registry, ECS::EntityId Entity, int NewIndex) {
    WeaponSwitcher& switcher = Registry.Get<WeaponSwitcher>(Entity);
    switcher.CurrentWeaponIndex = NewIndex;
    switcher.IsSwitching = true;
    switcher.SwitchTimer = 0.0f;
}

} // namespace Solstice::Game
