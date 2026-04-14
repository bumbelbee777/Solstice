#include "StrategyGame.hxx"
#include "../../Core/Debug/Debug.hxx"

namespace Solstice::Game {

StrategyGame::StrategyGame() {
    m_Camera = std::make_unique<StrategyCamera>();
    m_HUD = std::make_unique<HUD>();
}

void StrategyGame::Initialize() {
    GameBase::Initialize();
    InitializeCamera();
    InitializeSelectionSystem();
    InitializeResourceSystem();
}

void StrategyGame::InitializeCamera() {
    m_Camera->SetHeight(20.0f);
    m_Camera->SetAngle(45.0f);
    SIMPLE_LOG("StrategyGame: Camera initialized");
}

void StrategyGame::InitializeSelectionSystem() {
    SIMPLE_LOG("StrategyGame: Selection system initialized");
}

void StrategyGame::InitializeResourceSystem() {
    SIMPLE_LOG("StrategyGame: Resource system initialized");
}

void StrategyGame::Update(float DeltaTime) {
    GameBase::Update(DeltaTime);
    if (m_Camera) {
        m_Camera->Update(DeltaTime);
    }
    UpdateFogOfWar();
}

void StrategyGame::Render() {
    GameBase::Render();
    if (m_HUD && m_Window) {
        auto size = m_Window->GetFramebufferSize();
        m_HUD->Render(m_Registry, m_Camera->GetCamera(), size.first, size.second);
    }
}

void StrategyGame::SelectUnit(ECS::EntityId Entity) {
    auto it = std::find(m_SelectedUnits.begin(), m_SelectedUnits.end(), Entity);
    if (it == m_SelectedUnits.end()) {
        m_SelectedUnits.push_back(Entity);
    }
}

void StrategyGame::DeselectAll() {
    m_SelectedUnits.clear();
}

void StrategyGame::AddResource(const std::string& ResourceName, float Amount) {
    m_Resources[ResourceName] += Amount;
}

float StrategyGame::GetResource(const std::string& ResourceName) const {
    auto it = m_Resources.find(ResourceName);
    return (it != m_Resources.end()) ? it->second : 0.0f;
}

bool StrategyGame::ConsumeResource(const std::string& ResourceName, float Amount) {
    auto it = m_Resources.find(ResourceName);
    if (it != m_Resources.end() && it->second >= Amount) {
        it->second -= Amount;
        return true;
    }
    return false;
}

bool StrategyGame::CanPlaceBuilding(const Math::Vec3& Position, uint32_t BuildingType) const {
    (void)BuildingType;
    // Check if position is valid
    for (const auto& building : m_Buildings) {
        if ((building.first - Position).Magnitude() < 2.0f) {
            return false;
        }
    }
    return true;
}

void StrategyGame::PlaceBuilding(const Math::Vec3& Position, uint32_t BuildingType) {
    if (CanPlaceBuilding(Position, BuildingType)) {
        m_Buildings.push_back({Position, BuildingType});
    }
}

void StrategyGame::UpdateFogOfWar() {
    // Update fog of war based on unit positions
}

bool StrategyGame::IsPositionVisible(const Math::Vec3& Position) const {
    (void)Position;
    // Check if position is in fog of war
    return true;
}

} // namespace Solstice::Game
