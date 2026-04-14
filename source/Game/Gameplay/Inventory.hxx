#pragma once

#include "../../Solstice.hxx"
#include "../../Entity/Registry.hxx"
#include <string>
#include <vector>
#include <unordered_map>

namespace Solstice::Game {

// Item types
enum class ItemType {
    Weapon,
    Armor,
    Consumable,
    Material,
    Quest,
    Misc
};

// Item structure
struct Item {
    uint32_t ItemID{0};
    std::string Name;
    std::string Description;
    ItemType Type{ItemType::Misc};
    uint32_t Quantity{1};
    uint32_t MaxStack{1};
    float Weight{0.0f};

    // Item properties (flexible key-value store)
    std::unordered_map<std::string, float> Properties;
};

// Equipment slot types
enum class EquipmentSlot {
    Weapon,
    Head,
    Chest,
    Legs,
    Feet,
    Accessory1,
    Accessory2,
    COUNT
};

// Inventory component
struct Inventory {
    std::vector<Item> Items;
    std::unordered_map<EquipmentSlot, uint32_t> Equipment; // EquipmentSlot -> ItemID
    float MaxWeight{100.0f};
    uint32_t MaxSlots{50};
    float CurrentWeight{0.0f};
};

// Inventory system
class SOLSTICE_API InventorySystem {
public:
    // Add item to inventory
    static bool AddItem(ECS::Registry& Registry, ECS::EntityId Entity, const Item& Item);

    // Remove item from inventory
    static bool RemoveItem(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t ItemID, uint32_t Quantity = 1);

    // Get item from inventory
    static Item* GetItem(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t ItemID);

    // Check if inventory has item
    static bool HasItem(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t ItemID, uint32_t Quantity = 1);

    // Equip item
    static bool EquipItem(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t ItemID, EquipmentSlot Slot);

    // Unequip item
    static bool UnequipItem(ECS::Registry& Registry, ECS::EntityId Entity, EquipmentSlot Slot);

    // Get equipped item
    static Item* GetEquippedItem(ECS::Registry& Registry, ECS::EntityId Entity, EquipmentSlot Slot);

    // Use consumable item
    static bool UseItem(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t ItemID);

    // Update inventory (recalculate weight, etc.)
    static void Update(ECS::Registry& Registry, float DeltaTime);

private:
    static void RecalculateWeight(Inventory& Inventory);
    static bool CanAddItem(const Inventory& Inventory, const Item& Item);
};

} // namespace Solstice::Game
