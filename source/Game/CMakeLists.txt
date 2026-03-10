# Game: game loop, controllers, HUD, menus, etc.
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(Game STATIC
    GameBase.cxx
    InputManager.cxx
    SceneManager.cxx
    ComponentFactory.cxx
    GameState.cxx
    FPS/FPSGame.cxx
    FPS/FPSSandbox.cxx
    FPS/FPSMovement.cxx
    FPS/FirstPersonController.cxx
    FPS/Weapon.cxx
    FPS/WeaponSwitcher.cxx
    FPS/SnowSystem.cxx
    ThirdPerson/ThirdPersonGame.cxx
    ThirdPerson/ThirdPersonController.cxx
    Strategy/StrategyGame.cxx
    Strategy/StrategyCamera.cxx
    Health.cxx
    Inventory.cxx
    HUD.cxx
    Enemy.cxx
    Boss.cxx
    LoadingScreen.cxx
    PauseMenu.cxx
    GamePreferences.cxx
    MainMenu.cxx
    Autosave.cxx
    LevelSelector.cxx
    Difficulty.cxx
    SFXManager.cxx
    NPC.cxx
    AIBrain.cxx
    Stamina.cxx
    ScriptManager.cxx
    ProceduralTextureManager.cxx
    SceneBuilder.cxx
    MaterialPresetManager.cxx
    HUDPresets.cxx
    LevelBase.cxx
    DialogueTree.cxx
    DialoguePresenter.cxx
    VisualNovelPresets.cxx
    VisualNovel/VisualNovelGame.cxx
)
target_include_directories(Game PUBLIC ${SOURCE_DIR})
target_link_libraries(Game PUBLIC Core Math Physics Render UI Arzachel Scripting)
if(MSVC)
    target_compile_options(Game PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(Game PRIVATE SOLSTICE_EXPORTS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
