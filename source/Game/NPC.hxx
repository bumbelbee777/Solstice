#pragma once

#include "../Solstice.hxx"
#include "Enemy.hxx"
#include "../Entity/Registry.hxx"
#include "../Core/FSM.hxx"
#include "../Math/Vector.hxx"
#include <string>
#include <unordered_map>

namespace Solstice::Game {

// NPC types
enum class NPCType {
    Friendly,
    Neutral
};

// NPC roles
enum class NPCRole {
    Ally,
    Merchant,
    QuestGiver,
    Passive,
    Reactive
};

// NPC states
enum class NPCState {
    Idle,
    Wander,
    Follow,
    Trade,
    Dialogue,
    Flee
};

// NPC component extending Enemy
struct NPC : public Enemy {
    NPCType Type{NPCType::Friendly};
    NPCRole Role{NPCRole::Ally};

    // Dialogue
    uint32_t DialogueTreeID{0};
    bool IsInDialogue{false};

    // Trading
    bool CanTrade{false};
    std::unordered_map<uint32_t, float> TradePrices; // ItemID -> price

    // Relationships
    float Reputation{0.0f}; // -100 to 100
    float TrustLevel{0.5f}; // 0.0 to 1.0

    // Quest system
    std::vector<uint32_t> AvailableQuests;
    std::vector<uint32_t> CompletedQuests;

    // Behavior
    NPCState CurrentState{NPCState::Idle};
    float WanderRadius{5.0f};
    Math::Vec3 WanderCenter{0, 0, 0};
    ECS::EntityId FollowTarget{0};

    // FSM for state management
    std::shared_ptr<Core::State> StateMachine;
};

// NPC system
class SOLSTICE_API NPCSystem {
public:
    // Update NPC AI (call each frame)
    static void Update(ECS::Registry& Registry, float DeltaTime);

    // Set NPC state
    static void SetState(ECS::Registry& Registry, ECS::EntityId Entity, NPCState State);
    static NPCState GetState(ECS::Registry& Registry, ECS::EntityId Entity);

    // Dialogue
    static void StartDialogue(ECS::Registry& Registry, ECS::EntityId Entity, ECS::EntityId PlayerEntity);
    static void EndDialogue(ECS::Registry& Registry, ECS::EntityId Entity);

    // Trading
    static bool CanTrade(ECS::Registry& Registry, ECS::EntityId Entity);
    static bool TradeItem(ECS::Registry& Registry, ECS::EntityId Entity, ECS::EntityId PlayerEntity,
                         uint32_t ItemID, int Quantity);

    // Relationships
    static void ModifyReputation(ECS::Registry& Registry, ECS::EntityId Entity, float Amount);
    static float GetReputation(ECS::Registry& Registry, ECS::EntityId Entity);

    // Quest system
    static void GiveQuest(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t QuestID);
    static void CompleteQuest(ECS::Registry& Registry, ECS::EntityId Entity, uint32_t QuestID);

private:
    static void UpdateState(ECS::Registry& Registry, ECS::EntityId Entity, NPC& NPC, float DeltaTime);
    static void UpdateIdle(ECS::Registry& Registry, ECS::EntityId Entity, NPC& NPC, float DeltaTime);
    static void UpdateWander(ECS::Registry& Registry, ECS::EntityId Entity, NPC& NPC, float DeltaTime);
    static void UpdateFollow(ECS::Registry& Registry, ECS::EntityId Entity, NPC& NPC, float DeltaTime);
    static void UpdateFlee(ECS::Registry& Registry, ECS::EntityId Entity, NPC& NPC, float DeltaTime);
};

} // namespace Solstice::Game
