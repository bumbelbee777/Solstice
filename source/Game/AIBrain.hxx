#pragma once

#include "../Solstice.hxx"
#include "../Core/MLP.hxx"
#include "../Core/CNN.hxx"
#include "../Entity/Registry.hxx"
#include "../Math/Vector.hxx"
#include <memory>
#include <vector>

namespace Solstice::Game {

// AI brain component using MLP/CNN
struct AIBrain {
    std::unique_ptr<Core::MLP> MLPNetwork;
    std::unique_ptr<Core::CNN> CNNNetwork;

    // Input features (normalized)
    std::vector<float> InputFeatures;
    std::vector<float> OutputActions;

    // Feature extraction
    float DistanceToTarget{0.0f};
    float HealthPercent{1.0f};
    float AmmoPercent{1.0f};
    float ThreatLevel{0.0f};
    Math::Vec3 EnvironmentalFactors{0, 0, 0};

    // Decision making
    bool UseMLP{true}; // If false, use CNN
    float DecisionInterval{0.1f}; // Seconds between decisions
    float DecisionTimer{0.0f};

    // Behavior tree fallback
    bool UseBehaviorTree{false};
    std::string CurrentBehavior{"patrol"};
};

// AI system
class SOLSTICE_API AISystem {
public:
    // Update AI (call each frame)
    static void Update(ECS::Registry& Registry, float DeltaTime);

    // Initialize AI brain
    static void InitializeBrain(ECS::Registry& Registry, ECS::EntityId Entity, bool UseMLP = true);

    // Extract features from environment
    static void ExtractFeatures(ECS::Registry& Registry, ECS::EntityId Entity, AIBrain& Brain);

    // Make decision using neural network
    static void MakeDecision(ECS::Registry& Registry, ECS::EntityId Entity, AIBrain& Brain);

    // Apply decision to entity
    static void ApplyDecision(ECS::Registry& Registry, ECS::EntityId Entity, AIBrain& Brain);

private:
    static void UpdateMLP(AIBrain& Brain);
    static void UpdateCNN(AIBrain& Brain);
    static void UpdateBehaviorTree(ECS::Registry& Registry, ECS::EntityId Entity, AIBrain& Brain);
};

} // namespace Solstice::Game
