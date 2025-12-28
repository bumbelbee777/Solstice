#pragma once

#include "../Solstice.hxx"
#include "../Entity/Registry.hxx"
#include "Weapon.hxx"
#include "Inventory.hxx"
#include "../UI/Widgets.hxx"
#include <vector>
#include <string>

namespace Solstice::Game {

// Weapon switcher component
struct WeaponSwitcher {
    std::vector<uint32_t> WeaponInventory; // ItemIDs of weapons
    int CurrentWeaponIndex{-1}; // -1 means no weapon equipped
    float SwitchTime{0.3f}; // Time to switch weapons
    float SwitchTimer{0.0f};
    bool IsSwitching{false};

    // Quick switch keys (1-9)
    std::vector<int> QuickSwitchKeys{2, 3, 4, 5, 6, 7, 8, 9, 10}; // Keys 1-9
};

// Weapon switcher system
class SOLSTICE_API WeaponSwitcherSystem {
public:
    // Update (call each frame)
    static void Update(ECS::Registry& Registry, float DeltaTime);

    // Switch to weapon by index
    static bool SwitchToWeapon(ECS::Registry& Registry, ECS::EntityId Entity, int WeaponIndex);

    // Switch to next/previous weapon
    static bool SwitchToNextWeapon(ECS::Registry& Registry, ECS::EntityId Entity);
    static bool SwitchToPreviousWeapon(ECS::Registry& Registry, ECS::EntityId Entity);

    // Add weapon to inventory
    static bool AddWeapon(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t WeaponItemID);

    // Remove weapon from inventory
    static bool RemoveWeapon(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t WeaponItemID);

    // Get current weapon
    static Weapon* GetCurrentWeapon(ECS::Registry& Registry, ECS::EntityId Entity);
    static int GetCurrentWeaponIndex(ECS::Registry& Registry, ECS::EntityId Entity);

    // Handle input (number keys, scroll wheel)
    static void HandleInput(ECS::Registry& Registry, ECS::EntityId Entity, int KeyCode, float ScrollDelta);

    // Render weapon wheel UI
    static void RenderWeaponWheel(ECS::Registry& Registry, ECS::EntityId Entity, int ScreenWidth, int ScreenHeight);

private:
    static bool CanSwitch(ECS::Registry& Registry, ECS::EntityId Entity);
    static void PerformSwitch(ECS::Registry& Registry, ECS::EntityId Entity, int NewIndex);
};

} // namespace Solstice::Game
