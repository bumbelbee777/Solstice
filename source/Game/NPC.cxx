#include "NPC.hxx"
#include "Inventory.hxx"
#include "../Core/Debug.hxx"
#include "../Entity/Transform.hxx"
#include "../Physics/RigidBody.hxx"
#include <algorithm>

namespace Solstice::Game {

void NPCSystem::Update(ECS::Registry& Registry, float DeltaTime) {
    Registry.ForEach<NPC>([&](ECS::EntityId entity, NPC& npc) {
        UpdateState(Registry, entity, npc, DeltaTime);
    });
}

void NPCSystem::SetState(ECS::Registry& Registry, ECS::EntityId Entity, NPCState State) {
    if (!Registry.Has<NPC>(Entity)) return;
    Registry.Get<NPC>(Entity).CurrentState = State;
}

NPCState NPCSystem::GetState(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<NPC>(Entity)) return NPCState::Idle;
    return Registry.Get<NPC>(Entity).CurrentState;
}

void NPCSystem::StartDialogue(ECS::Registry& Registry, ECS::EntityId Entity, ECS::EntityId PlayerEntity) {
    if (!Registry.Has<NPC>(Entity)) return;

    NPC& npc = Registry.Get<NPC>(Entity);
    npc.IsInDialogue = true;
    npc.CurrentState = NPCState::Dialogue;
}

void NPCSystem::EndDialogue(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<NPC>(Entity)) return;

    NPC& npc = Registry.Get<NPC>(Entity);
    npc.IsInDialogue = false;
    npc.CurrentState = NPCState::Idle;
}

bool NPCSystem::CanTrade(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<NPC>(Entity)) return false;
    NPC& npc = Registry.Get<NPC>(Entity);
    return npc.CanTrade && npc.Role == NPCRole::Merchant;
}

bool NPCSystem::TradeItem(ECS::Registry& Registry, ECS::EntityId Entity, ECS::EntityId PlayerEntity,
                          uint32_t ItemID, int Quantity) {
    if (!CanTrade(Registry, Entity)) return false;
    if (!Registry.Has<Inventory>(PlayerEntity)) return false;

    NPC& npc = Registry.Get<NPC>(Entity);
    auto it = npc.TradePrices.find(ItemID);
    if (it == npc.TradePrices.end()) return false;

    float totalPrice = it->second * static_cast<float>(Quantity);
    // Would check player has enough currency, then transfer items
    // For now, just return true
    return true;
}

void NPCSystem::ModifyReputation(ECS::Registry& Registry, ECS::EntityId Entity, float Amount) {
    if (!Registry.Has<NPC>(Entity)) return;
    NPC& npc = Registry.Get<NPC>(Entity);
    npc.Reputation = std::max(-100.0f, std::min(100.0f, npc.Reputation + Amount));
}

float NPCSystem::GetReputation(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<NPC>(Entity)) return 0.0f;
    return Registry.Get<NPC>(Entity).Reputation;
}

void NPCSystem::GiveQuest(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t QuestID) {
    if (!Registry.Has<NPC>(Entity)) return;
    NPC& npc = Registry.Get<NPC>(Entity);
    auto it = std::find(npc.AvailableQuests.begin(), npc.AvailableQuests.end(), QuestID);
    if (it == npc.AvailableQuests.end()) {
        npc.AvailableQuests.push_back(QuestID);
    }
}

void NPCSystem::CompleteQuest(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t QuestID) {
    if (!Registry.Has<NPC>(Entity)) return;
    NPC& npc = Registry.Get<NPC>(Entity);
    auto it = std::find(npc.AvailableQuests.begin(), npc.AvailableQuests.end(), QuestID);
    if (it != npc.AvailableQuests.end()) {
        npc.AvailableQuests.erase(it);
        npc.CompletedQuests.push_back(QuestID);
    }
}

void NPCSystem::UpdateState(ECS::Registry& Registry, ECS::EntityId Entity, NPC& NPC, float DeltaTime) {
    switch (NPC.CurrentState) {
        case NPCState::Idle:
            UpdateIdle(Registry, Entity, NPC, DeltaTime);
            break;
        case NPCState::Wander:
            UpdateWander(Registry, Entity, NPC, DeltaTime);
            break;
        case NPCState::Follow:
            UpdateFollow(Registry, Entity, NPC, DeltaTime);
            break;
        case NPCState::Flee:
            UpdateFlee(Registry, Entity, NPC, DeltaTime);
            break;
        case NPCState::Dialogue:
        case NPCState::Trade:
            // These states are handled by other systems
            break;
    }
}

void NPCSystem::UpdateIdle(ECS::Registry& Registry, ECS::EntityId Entity, NPC& NPC, float DeltaTime) {
    (void)Registry;
    (void)Entity;
    (void)DeltaTime;
    // Idle behavior
}

void NPCSystem::UpdateWander(ECS::Registry& Registry, ECS::EntityId Entity, NPC& NPC, float DeltaTime) {
    if (!Registry.Has<Physics::RigidBody>(Entity)) return;

    auto& rb = Registry.Get<Physics::RigidBody>(Entity);
    // Simple wander: move randomly within radius
    // Would implement proper wander behavior
}

void NPCSystem::UpdateFollow(ECS::Registry& Registry, ECS::EntityId Entity, NPC& NPC, float DeltaTime) {
    if (NPC.FollowTarget == 0) return;
    if (!Registry.Has<Physics::RigidBody>(Entity)) return;
    if (!Registry.Has<ECS::Transform>(NPC.FollowTarget)) return;

    auto& rb = Registry.Get<Physics::RigidBody>(Entity);
    auto& targetTransform = Registry.Get<ECS::Transform>(NPC.FollowTarget);

    Math::Vec3 direction = (targetTransform.Position - rb.Position).Normalized();
    rb.Velocity.x = direction.x * NPC.MoveSpeed;
    rb.Velocity.z = direction.z * NPC.MoveSpeed;
}

void NPCSystem::UpdateFlee(ECS::Registry& Registry, ECS::EntityId Entity, NPC& NPC, float DeltaTime) {
    if (NPC.TargetEntity == 0) return;
    if (!Registry.Has<Physics::RigidBody>(Entity)) return;
    if (!Registry.Has<ECS::Transform>(NPC.TargetEntity)) return;

    auto& rb = Registry.Get<Physics::RigidBody>(Entity);
    auto& targetTransform = Registry.Get<ECS::Transform>(NPC.TargetEntity);

    Math::Vec3 direction = (rb.Position - targetTransform.Position).Normalized();
    rb.Velocity.x = direction.x * NPC.MoveSpeed * 1.5f; // Flee faster
    rb.Velocity.z = direction.z * NPC.MoveSpeed * 1.5f;
}

} // namespace Solstice::Game
