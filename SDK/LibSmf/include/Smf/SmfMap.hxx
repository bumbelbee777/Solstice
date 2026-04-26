#pragma once

#include "SmfSpatial.hxx"
#include "SmfTypes.hxx"

#include <string>
#include <utility>
#include <vector>

namespace Solstice::Smf {

struct SmfProperty {
    std::string Key;
    SmfAttributeType Type{SmfAttributeType::Float};
    SmfValue Value;
};

struct SmfEntity {
    std::string Name;
    std::string ClassName;
    std::vector<SmfProperty> Properties;
};

/// Authoring-time acoustic zone; fields align with ``Core::Audio::AcousticZone`` / JSON map tooling.
struct SmfAcousticZone {
    std::string Name;
    SmfVec3 Center{};
    SmfVec3 Extents{5.f, 5.f, 5.f};
    /// ``None``, ``Room``, ``Cave``, ``Hallway``, ``Sewer``, or ``Industrial``.
    std::string ReverbPreset{"Room"};
    float Wetness{0.35f};
    float ObstructionMultiplier{1.f};
    int32_t Priority{0};
    bool IsSpherical{false};
    bool Enabled{true};
    /// Optional path (relative to the .smf directory, path-table / engine resolution). Empty = none.
    std::string MusicPath;
    /// Optional looping 3D ambience bed at zone center. Empty = none.
    std::string AmbiencePath;
};

enum class SmfAuthoringLightType : uint8_t {
    Point = 0,
    Spot = 1,
    Directional = 2,
};

/// Authoring light; core fields align with ``Physics::LightSource`` (plus spot cone and direction for spot/directional).
struct SmfAuthoringLight {
    std::string Name;
    SmfAuthoringLightType Type{SmfAuthoringLightType::Point};
    SmfVec3 Position{};
    /// Used for spot / directional (world-space forward / axis).
    SmfVec3 Direction{0.f, -1.f, 0.f};
    SmfVec3 Color{1.f, 1.f, 1.f};
    float Intensity{1.f};
    float Hue{0.f};
    float Attenuation{1.f};
    /// 0 = infinite range (engine convention).
    float Range{0.f};
    float SpotInnerDeg{30.f};
    float SpotOuterDeg{45.f};
};

/// Optional cubemap sky (six face paths + authoring knobs). Stored in **SMAL v1** gameplay extras (see `SmfBinary.cxx`).
struct SmfSkybox {
    bool Enabled{false};
    float Brightness{1.f};
    /// World Y-up: rotates the skybox around +Y (degrees).
    float YawDegrees{0.f};
    /// Order: +X, −X, +Y, −Y, +Z, −Z (relative paths preferred).
    std::string FacePaths[6];
};

/// Path-only hooks for runtime/script integration (no embedded script bodies). Stored in **SMAL v1** after skybox.
struct SmfWorldAuthoringHooks {
    std::string ScriptPath;
    std::string CutscenePath;
    std::string WorldSpaceUiPath;
};

/// Authoring-time Navier-Stokes volume for **`NSSolver` / `FluidSimulation`**: world AABB + coarse grid only (no dense field data in .smf).
/// Runtime clamps resolution to stay within **kSmfFluidInteriorCellBudget** for stability and memory.
struct SmfFluidVolume {
    std::string Name;
    bool Enabled{true};
    bool EnableMacCormack{true};
    bool EnableBoussinesq{false};
    /// When true, visualization may clip to **BoundsMin/Max** (physics grid still fills the box).
    bool VolumeVisualizationClip{false};
    SmfVec3 BoundsMin{};
    SmfVec3 BoundsMax{1.f, 1.f, 1.f};
    int32_t ResolutionX{32};
    int32_t ResolutionY{32};
    int32_t ResolutionZ{32};
    float Diffusion{0.0001f};
    float Viscosity{0.0001f};
    float ReferenceDensity{1000.f};
    int32_t PressureRelaxationIterations{32};
    float BuoyancyStrength{1.f};
    float Prandtl{0.71f};
};

inline constexpr int kSmfFluidResolutionMin = 4;
inline constexpr int kSmfFluidResolutionMax = 128;
inline constexpr int64_t kSmfFluidInteriorCellBudget = 262144;

struct SmfMap {
    std::vector<SmfEntity> Entities;
    std::vector<std::pair<std::string, uint64_t>> PathTable;

    std::optional<SmfAuthoringBsp> Bsp;
    std::optional<SmfAuthoringOctree> Octree;

    std::vector<SmfAcousticZone> AcousticZones;
    std::vector<SmfAuthoringLight> AuthoringLights;

    std::optional<SmfSkybox> Skybox;

    SmfWorldAuthoringHooks WorldAuthoringHooks;

    std::vector<SmfFluidVolume> FluidVolumes;

    /// Optional path to a **baked** RGBA lightmap (relative to the .smf; optional **BKM1** tag in **SMAL** after world hooks).
    std::string BakedLightmapPath;

    void Clear() {
        Entities.clear();
        PathTable.clear();
        Bsp.reset();
        Octree.reset();
        AcousticZones.clear();
        AuthoringLights.clear();
        Skybox.reset();
        WorldAuthoringHooks = {};
        FluidVolumes.clear();
        BakedLightmapPath.clear();
    }
};

} // namespace Solstice::Smf
