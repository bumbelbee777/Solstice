#include "Inventory.hxx"
#include "Health.hxx"
#include "Core/Debug.hxx"
#include <algorithm>

namespace Solstice::Game {

bool InventorySystem::AddItem(ECS::Registry& Registry, ECS::EntityId Entity, const Item& Item) {
    if (!Registry.Has<Inventory>(Entity)) return false;

    auto& inventory = Registry.Get<Inventory>(Entity);

    if (!CanAddItem(inventory, Item)) {
        return false;
    }

    // Try to stack with existing item
    if (Item.MaxStack > 1) {
        for (auto& existingItem : inventory.Items) {
            if (existingItem.ItemID == Item.ItemID && existingItem.Quantity < existingItem.MaxStack) {
                uint32_t spaceLeft = existingItem.MaxStack - existingItem.Quantity;
                uint32_t toAdd = std::min(Item.Quantity, spaceLeft);
                existingItem.Quantity += toAdd;

                if (toAdd < Item.Quantity) {
                    // Add remaining as new stack
                    Game::Item newItem = Item;
                    newItem.Quantity = Item.Quantity - toAdd;
                    inventory.Items.push_back(newItem);
                }

                RecalculateWeight(inventory);
                return true;
            }
        }
    }

    // Add as new item
    inventory.Items.push_back(Item);
    RecalculateWeight(inventory);
    return true;
}

bool InventorySystem::RemoveItem(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t ItemID, uint32_t Quantity) {
    if (!Registry.Has<Inventory>(Entity)) return false;

    auto& inventory = Registry.Get<Inventory>(Entity);
    uint32_t remaining = Quantity;

    auto it = inventory.Items.begin();
    while (it != inventory.Items.end() && remaining > 0) {
        if (it->ItemID == ItemID) {
            if (it->Quantity <= remaining) {
                remaining -= it->Quantity;
                it = inventory.Items.erase(it);
            } else {
                it->Quantity -= remaining;
                remaining = 0;
                ++it;
            }
        } else {
            ++it;
        }
    }

    if (remaining == 0) {
        RecalculateWeight(inventory);
        return true;
    }

    return false;
}

Item* InventorySystem::GetItem(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t ItemID) {
    if (!Registry.Has<Inventory>(Entity)) return nullptr;

    auto& inventory = Registry.Get<Inventory>(Entity);
    for (auto& item : inventory.Items) {
        if (item.ItemID == ItemID) {
            return &item;
        }
    }
    return nullptr;
}

bool InventorySystem::HasItem(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t ItemID, uint32_t Quantity) {
    if (!Registry.Has<Inventory>(Entity)) return false;

    auto& inventory = Registry.Get<Inventory>(Entity);
    uint32_t total = 0;

    for (const auto& item : inventory.Items) {
        if (item.ItemID == ItemID) {
            total += item.Quantity;
            if (total >= Quantity) {
                return true;
            }
        }
    }

    return false;
}

bool InventorySystem::EquipItem(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t ItemID, EquipmentSlot Slot) {
    if (!Registry.Has<Inventory>(Entity)) return false;

    auto& inventory = Registry.Get<Inventory>(Entity);
    Item* item = GetItem(Registry, Entity, ItemID);

    if (!item) return false;

    // Unequip current item in slot if any
    UnequipItem(Registry, Entity, Slot);

    // Equip new item
    inventory.Equipment[Slot] = ItemID;
    return true;
}

bool InventorySystem::UnequipItem(ECS::Registry& Registry, ECS::EntityId Entity, EquipmentSlot Slot) {
    if (!Registry.Has<Inventory>(Entity)) return false;

    auto& inventory = Registry.Get<Inventory>(Entity);
    auto it = inventory.Equipment.find(Slot);

    if (it != inventory.Equipment.end()) {
        inventory.Equipment.erase(it);
        return true;
    }

    return false;
}

Item* InventorySystem::GetEquippedItem(ECS::Registry& Registry, ECS::EntityId Entity, EquipmentSlot Slot) {
    if (!Registry.Has<Inventory>(Entity)) return nullptr;

    auto& inventory = Registry.Get<Inventory>(Entity);
    auto it = inventory.Equipment.find(Slot);

    if (it != inventory.Equipment.end()) {
        return GetItem(Registry, Entity, it->second);
    }

    return nullptr;
}

bool InventorySystem::UseItem(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t ItemID) {
    if (!Registry.Has<Inventory>(Entity)) return false;

    Item* item = GetItem(Registry, Entity, ItemID);
    if (!item || item->Type != ItemType::Consumable) return false;

    // Apply consumable effects (e.g., healing)
    if (item->Properties.find("HealAmount") != item->Properties.end()) {
        float healAmount = item->Properties.at("HealAmount");
        HealthSystem::ApplyHealing(Registry, Entity, healAmount);
    }

    // Remove one from stack
    RemoveItem(Registry, Entity, ItemID, 1);
    return true;
}

void InventorySystem::Update(ECS::Registry& Registry, float DeltaTime) {
    (void)DeltaTime;
    Registry.ForEach<Inventory>([&](ECS::EntityId entity, Inventory& inventory) {
        RecalculateWeight(inventory);
    });
}

void InventorySystem::RecalculateWeight(Inventory& Inventory) {
    Inventory.CurrentWeight = 0.0f;
    for (const auto& item : Inventory.Items) {
        Inventory.CurrentWeight += item.Weight * static_cast<float>(item.Quantity);
    }
}

bool InventorySystem::CanAddItem(const Inventory& Inventory, const Item& Item) {
    // Check slot limit
    if (Inventory.Items.size() >= Inventory.MaxSlots && Item.MaxStack == 1) {
        return false;
    }

    // Check weight limit
    float newWeight = Inventory.CurrentWeight + (Item.Weight * static_cast<float>(Item.Quantity));
    if (newWeight > Inventory.MaxWeight) {
        return false;
    }

    return true;
}

} // namespace Solstice::Game
