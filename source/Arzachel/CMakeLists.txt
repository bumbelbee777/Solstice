# Arzachel: procedural generation, meshes, animation clips, rigging
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(Arzachel STATIC
    Seed.cxx
    Generator.cxx
    MeshData.cxx
    GeometryOps.cxx
    Constraints.cxx
    AssetBuilder.cxx
    AssetPresets.cxx
    MaterialPresets.cxx
    Polyhedra.cxx
    SkinWeights.cxx
    RiggedMesh.cxx
    RiggingOps.cxx
    RiggedPrimitives.cxx
    MeshFactory.cxx
    ProceduralTexture.cxx
    MaterialSerializer.cxx
    AnimationTrack.cxx
    AnimationClip.cxx
    AnimationOps.cxx
    FacialAnimation.cxx
    TextLipSync.cxx
    GLTFImport.cxx
    GLTFExport.cxx
    TerrainGenerator.cxx
    StructureGenerator.cxx
    CharacterPrimitives.cxx
    MapSerializer.cxx
    Presets/Accessories/AccessoryPresets.cxx
    Presets/Weapons/WeaponPresets.cxx
    Presets/Bullets/BulletPresets.cxx
    Presets/Furniture/FurniturePresets.cxx
    Presets/Industrial/IndustrialPresets.cxx
    Presets/Architecture/ArchitecturePresets.cxx
    Presets/Characters/CharacterPresets.cxx
    Presets/LevelCompositions.cxx
)
target_include_directories(Arzachel PUBLIC ${SOURCE_DIR})
target_link_libraries(Arzachel PUBLIC Math Core Material Skeleton MinGfx bgfx bimg bx LibSmf Physics)
if(MSVC)
    target_compile_options(Arzachel PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(Arzachel PRIVATE SOLSTICE_EXPORTS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
