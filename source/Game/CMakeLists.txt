# Game: game loop, controllers, HUD, menus, etc.
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(Game STATIC
    App/GameBase.cxx
    App/InputManager.cxx
    App/SceneManager.cxx
    App/GameState.cxx
    App/Autosave.cxx
    App/Difficulty.cxx
    App/GamePreferences.cxx
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
    UI/HUD.cxx
    UI/HUDPresets.cxx
    UI/MainMenu.cxx
    UI/PauseMenu.cxx
    UI/LoadingScreen.cxx
    UI/LevelSelector.cxx
    UI/DialoguePresenter.cxx
    Gameplay/ComponentFactory.cxx
    Gameplay/Health.cxx
    Gameplay/Inventory.cxx
    Gameplay/Enemy.cxx
    Gameplay/Boss.cxx
    Gameplay/SFXManager.cxx
    Gameplay/NPC.cxx
    Gameplay/Stamina.cxx
    AI/AIBrain.cxx
    World/ProceduralTextureManager.cxx
    World/SceneBuilder.cxx
    World/MaterialPresetManager.cxx
    World/LevelBase.cxx
    Integration/ScriptManager.cxx
    Dialogue/DialogueTree.cxx
    Dialogue/NarrativeRuntime.cxx
    Dialogue/NarrativeDocument.cxx
    Dialogue/NarrativeScriptBindings.cxx
    Dialogue/VisualNovelPresets.cxx
    Cutscene/CutscenePlayer.cxx
    VisualNovel/VisualNovelGame.cxx
)
target_include_directories(Game PUBLIC ${SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Game PUBLIC Core Math Physics Render UI Arzachel Scripting)
target_link_libraries(Game PRIVATE Entity)
if(MSVC)
    target_compile_options(Game PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(Game PRIVATE SOLSTICE_EXPORTS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
