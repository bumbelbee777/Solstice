#pragma once

#include "../App/GameBase.hxx"
#include "StrategyCamera.hxx"
#include "../UI/HUD.hxx"
#include "../../Entity/Registry.hxx"
#include "../../Math/Vector.hxx"
#include <vector>
#include <unordered_map>
#include <memory>

namespace Solstice::Game {

// Strategy game base class
class SOLSTICE_API StrategyGame : public GameBase {
public:
    StrategyGame();
    virtual ~StrategyGame() = default;

protected:
    void Initialize() override;
    void Update(float DeltaTime) override;
    void Render() override;

    // Strategy-specific initialization
    virtual void InitializeCamera();
    virtual void InitializeSelectionSystem();
    virtual void InitializeResourceSystem();

    // Selection
    void SelectUnit(ECS::EntityId Entity);
    void DeselectAll();
    std::vector<ECS::EntityId> GetSelectedUnits() const { return m_SelectedUnits; }

    // Resource management
    void AddResource(const std::string& ResourceName, float Amount);
    float GetResource(const std::string& ResourceName) const;
    bool ConsumeResource(const std::string& ResourceName, float Amount);

    // Building placement
    bool CanPlaceBuilding(const Math::Vec3& Position, uint32_t BuildingType) const;
    void PlaceBuilding(const Math::Vec3& Position, uint32_t BuildingType);

    // Fog of war
    void UpdateFogOfWar();
    bool IsPositionVisible(const Math::Vec3& Position) const;

    ECS::Registry m_Registry;
    std::unique_ptr<StrategyCamera> m_Camera;
    std::unique_ptr<HUD> m_HUD;

    std::vector<ECS::EntityId> m_SelectedUnits;
    std::unordered_map<std::string, float> m_Resources;
    std::vector<std::pair<Math::Vec3, uint32_t>> m_Buildings;
};

} // namespace Solstice::Game
