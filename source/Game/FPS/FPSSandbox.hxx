#pragma once

#include "FPSGame.hxx"
#include "../Gameplay/Inventory.hxx"
#include <vector>
#include <string>

namespace Solstice::Game {

// FPS sandbox game base class
class SOLSTICE_API FPSSandbox : public FPSGame {
public:
    FPSSandbox();
    virtual ~FPSSandbox() = default;

protected:
    void Initialize() override;

    // Sandbox-specific initialization
    virtual void InitializeBuildingSystem();
    virtual void InitializeResourceSystem();
    virtual void InitializeCraftingSystem();

    // Building/placement
    void PlaceBlock(const Math::Vec3& Position, uint32_t BlockType);
    void RemoveBlock(const Math::Vec3& Position);
    bool CanPlaceBlock(const Math::Vec3& Position) const;

    // Resources
    void AddResource(const std::string& ResourceName, float Amount);
    float GetResource(const std::string& ResourceName) const;
    bool ConsumeResource(const std::string& ResourceName, float Amount);

    // Crafting
    bool CanCraft(const std::string& RecipeName) const;
    bool Craft(const std::string& RecipeName);

private:
    std::unordered_map<std::string, float> m_Resources;
    std::vector<std::pair<Math::Vec3, uint32_t>> m_PlacedBlocks;
};

} // namespace Solstice::Game
