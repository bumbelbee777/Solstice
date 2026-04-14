#include "FPSSandbox.hxx"
#include "../../Core/Debug/Debug.hxx"

namespace Solstice::Game {

FPSSandbox::FPSSandbox() {
}

void FPSSandbox::Initialize() {
    FPSGame::Initialize();
    InitializeBuildingSystem();
    InitializeResourceSystem();
    InitializeCraftingSystem();
}

void FPSSandbox::InitializeBuildingSystem() {
    SIMPLE_LOG("FPSSandbox: Building system initialized");
}

void FPSSandbox::InitializeResourceSystem() {
    SIMPLE_LOG("FPSSandbox: Resource system initialized");
}

void FPSSandbox::InitializeCraftingSystem() {
    SIMPLE_LOG("FPSSandbox: Crafting system initialized");
}

void FPSSandbox::PlaceBlock(const Math::Vec3& Position, uint32_t BlockType) {
    m_PlacedBlocks.push_back({Position, BlockType});
}

void FPSSandbox::RemoveBlock(const Math::Vec3& Position) {
    // Remove block at position
    m_PlacedBlocks.erase(
        std::remove_if(m_PlacedBlocks.begin(), m_PlacedBlocks.end(),
            [&Position](const std::pair<Math::Vec3, uint32_t>& block) {
                return (block.first - Position).Magnitude() < 0.1f;
            }),
        m_PlacedBlocks.end()
    );
}

bool FPSSandbox::CanPlaceBlock(const Math::Vec3& Position) const {
    // Check if position is valid (not overlapping, etc.)
    for (const auto& block : m_PlacedBlocks) {
        if ((block.first - Position).Magnitude() < 0.5f) {
            return false;
        }
    }
    return true;
}

void FPSSandbox::AddResource(const std::string& ResourceName, float Amount) {
    m_Resources[ResourceName] += Amount;
}

float FPSSandbox::GetResource(const std::string& ResourceName) const {
    auto it = m_Resources.find(ResourceName);
    return (it != m_Resources.end()) ? it->second : 0.0f;
}

bool FPSSandbox::ConsumeResource(const std::string& ResourceName, float Amount) {
    auto it = m_Resources.find(ResourceName);
    if (it != m_Resources.end() && it->second >= Amount) {
        it->second -= Amount;
        return true;
    }
    return false;
}

bool FPSSandbox::CanCraft(const std::string& RecipeName) const {
    (void)RecipeName;
    // Would check recipe requirements
    return false;
}

bool FPSSandbox::Craft(const std::string& RecipeName) {
    if (CanCraft(RecipeName)) {
        // Consume resources and create item
        return true;
    }
    return false;
}

} // namespace Solstice::Game
