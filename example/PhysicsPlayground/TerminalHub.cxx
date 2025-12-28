#include "TerminalHub.hxx"
#include "ObjectSpawner.hxx"
#include <UI/UISystem.hxx>
#include <Render/Skybox.hxx>
#include <Render/ParticlePresets.hxx>
#include <Core/Material.hxx>
#include <Arzachel/MeshFactory.hxx>
#include <Game/InputManager.hxx>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace Solstice;
using namespace Solstice::Math;
using namespace Solstice::Render;
using namespace Solstice::UI;

namespace Solstice::PhysicsPlayground {

TerminalHub::TerminalHub(
    Render::Scene& scene,
    Render::MeshLibrary& meshLibrary,
    Core::MaterialLibrary& materialLibrary,
    ECS::Registry& registry,
    Render::Skybox* skybox,
    Render::Camera& camera
) : m_Scene(scene), m_MeshLibrary(meshLibrary), m_MaterialLibrary(materialLibrary),
    m_Registry(registry), m_Skybox(skybox), m_Camera(camera) {

    // Create 5 terminals in a semi-circle near spawn (0, 0, 0)
    const float radius = 3.0f;  // Smaller radius, closer to spawn
    const float centerX = 0.0f;
    const float centerY = 0.5f;  // On ground
    const float centerZ = -15.0f;  // Moved further back to avoid spawn area conflicts

    // Terminal positions (semi-circle)
    std::vector<Math::Vec3> positions = {
        Math::Vec3(centerX - radius * 0.8f, centerY, centerZ - radius * 0.6f), // Time of Day
        Math::Vec3(centerX - radius * 0.4f, centerY, centerZ - radius * 0.9f), // Weather
        Math::Vec3(centerX, centerY, centerZ - radius), // Fluid
        Math::Vec3(centerX + radius * 0.4f, centerY, centerZ - radius * 0.9f), // Script
        Math::Vec3(centerX + radius * 0.8f, centerY, centerZ - radius * 0.6f)  // Environment
    };

    // Create a marker mesh (pyramid pointing down)
    uint32_t markerMeshID = m_MeshLibrary.AddMesh(Solstice::Arzachel::MeshFactory::CreatePyramid(0.4f, 0.6f));
    uint32_t markerMatID = m_MaterialLibrary.AddMaterial(Core::Materials::CreateEmissive(Math::Vec3(0.0f, 1.0f, 1.0f), 2.0f));

    // Create terminal base mesh (1x1x1 cube)
    uint32_t terminalMeshID = m_MeshLibrary.AddMesh(Solstice::Arzachel::MeshFactory::CreateCube(1.0f));
    uint32_t terminalMatID = m_MaterialLibrary.AddMaterial(Core::Materials::CreateMetal(Math::Vec3(0.2f, 0.2f, 0.2f), 0.6f));
    // Apply material to mesh immediately as it is shared
    if (auto* tMesh = m_MeshLibrary.GetMesh(terminalMeshID)) {
        if (!tMesh->SubMeshes.empty()) tMesh->SubMeshes[0].MaterialID = terminalMatID;
    }

    // Create terminals and add markers to scene
    for (size_t i = 0; i < positions.size(); ++i) {
        TerminalType type = static_cast<TerminalType>(i);
        m_Terminals.emplace_back(type, positions[i], 400.0f, 350.0f);
        
        // Add terminal visual base
        m_Scene.AddObject(terminalMeshID, positions[i], Math::Quaternion(), Math::Vec3(1, 1, 1), Render::ObjectType_Static);

        // Add visual indicator (rotated 180 degrees around X to point down)
        // Adjust position to be above the terminal mesh
        Math::Vec3 markerPos = positions[i] + Math::Vec3(0, 1.5f, 0);
        Math::Quaternion rotation = Math::Quaternion::FromEuler(180.0f, 0.0f, 0.0f);
        
        auto objID = m_Scene.AddObject(markerMeshID, markerPos, rotation, Math::Vec3(1, 1, 1), Render::ObjectType_Static);
        
        // Apply emissive material
        Render::Mesh* mesh = m_MeshLibrary.GetMesh(markerMeshID);
        if (mesh && !mesh->SubMeshes.empty()) {
            mesh->SubMeshes[0].MaterialID = markerMatID;
        }
    }
}

TerminalHub::~TerminalHub() {
}

void TerminalHub::Update(float deltaTime, const Render::Camera& camera, Solstice::Game::InputManager& inputManager) {
    (void)deltaTime;

    // Check if player is near any terminal
    Math::Vec3 playerPos = camera.Position;
    m_ActiveTerminal = -1;

    for (size_t i = 0; i < m_Terminals.size(); ++i) {
        if (IsNearTerminal(playerPos, m_Terminals[i].Position, 5.0f)) {
            m_ActiveTerminal = static_cast<int>(i);
            break;
        }
    }

    // Check for interaction (E key)
    if (m_ActiveTerminal >= 0 && inputManager.IsActionJustPressed(Solstice::Game::InputAction::Interact)) {
        m_Interacting = !m_Interacting;
        if (m_ActiveTerminal >= 0 && m_ActiveTerminal < static_cast<int>(m_Terminals.size())) {
            m_Terminals[m_ActiveTerminal].IsActive = m_Interacting;
        }
    }

    // ESC to exit interaction
    if (m_Interacting && inputManager.IsKeyJustPressed(27)) { // ESC
        m_Interacting = false;
        if (m_ActiveTerminal >= 0 && m_ActiveTerminal < static_cast<int>(m_Terminals.size())) {
            m_Terminals[m_ActiveTerminal].IsActive = false;
        }
    }
}

void TerminalHub::Render(const Render::Camera& camera, int screenWidth, int screenHeight) {
    (void)screenWidth; (void)screenHeight; // Unused, we use ImGui dimensions
    
    // Use ImGui dimensions for UI consistency
    auto& io = ImGui::GetIO();
    float uiWidth = io.DisplaySize.x;
    float uiHeight = io.DisplaySize.y;

    auto viewMatrix = camera.GetViewMatrix();
    auto projMatrix = Math::Matrix4::Perspective(
        camera.GetZoom() * 0.0174533f,
        uiWidth / uiHeight,
        0.1f,
        1000.0f
    );

    // Find closest terminal
    int closestTerminalVal = -1;
    float closestDist = 100.0f; // Max range to even consider
    
    // If we are actively interacting, we only care about that one
    if (m_Interacting && m_ActiveTerminal >= 0) {
        closestTerminalVal = m_ActiveTerminal;
    } else {
        // Otherwise find closest one to display label
        for (size_t i = 0; i < m_Terminals.size(); ++i) {
             float dist = (camera.Position - m_Terminals[i].Position).Magnitude();
             if (dist < closestDist) {
                 closestDist = dist;
                 closestTerminalVal = static_cast<int>(i);
             }
        }
    }

    // Nothing nearby to show
    if (closestTerminalVal < 0) return;

    // Render Label for the closest/active terminal
    const char* terminalNames[] = { "Time of Day", "Weather", "Fluid", "Script", "Environment" };
    // Only show if within 10 meters (already filtered by closestDist init above if interacting is false)
    bool showLabel = m_Interacting || closestDist < 10.0f;

    if (showLabel) {
        Terminal& t = m_Terminals[closestTerminalVal];
        Math::Vec3 labelPos = t.Position + Math::Vec3(0, 2.2f, 0);
        
        // Use standard projection with UI dimensions
        auto screenPos = UI::ViewportUI::ProjectToScreen(labelPos, viewMatrix, projMatrix, (int)uiWidth, (int)uiHeight);

        // Simple check if reasonable on screen
        if (screenPos.x > -100 && screenPos.x < uiWidth + 100 && screenPos.y > -100 && screenPos.y < uiHeight + 100) {
            std::string label = terminalNames[closestTerminalVal];
            if (m_Interacting && m_ActiveTerminal == closestTerminalVal) {
                label += " [Active]";
            } else if (IsNearTerminal(camera.Position, t.Position, 5.0f)) {
                 label += "\n[Press E]";
            }

            ImDrawList* drawList = ImGui::GetForegroundDrawList(); // Use Foreground to draw over everything
            ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
            // Draw background for readability
            drawList->AddRectFilled(
                ImVec2(screenPos.x - textSize.x * 0.5f - 4, screenPos.y - textSize.y * 0.5f - 4),
                ImVec2(screenPos.x + textSize.x * 0.5f + 4, screenPos.y + textSize.y * 0.5f + 4),
                IM_COL32(0, 0, 0, 180), 4.0f
            );
            drawList->AddText(
                ImVec2(screenPos.x - textSize.x * 0.5f, screenPos.y - textSize.y * 0.5f), 
                IM_COL32(255, 255, 255, 255), label.c_str()
            );
        }
    }

    // Now render the active interaction dialog
    if (!m_Interacting || m_ActiveTerminal < 0) return;

    Terminal& terminal = m_Terminals[m_ActiveTerminal];
    if (!terminal.Dialog) return;

    // Set content callback based on terminal type
    switch (terminal.Type) {
        case TerminalType::TimeOfDay:
            terminal.Dialog->SetContentCallback([this, &terminal, &camera, uiWidth, uiHeight]() {
                RenderTimeOfDayTerminal(terminal, camera, (int)uiWidth, (int)uiHeight);
            });
            break;
        case TerminalType::Weather:
            terminal.Dialog->SetContentCallback([this, &terminal, &camera, uiWidth, uiHeight]() {
                RenderWeatherTerminal(terminal, camera, (int)uiWidth, (int)uiHeight);
            });
            break;
        case TerminalType::Fluid:
            terminal.Dialog->SetContentCallback([this, &terminal, &camera, uiWidth, uiHeight]() {
                RenderFluidTerminal(terminal, camera, (int)uiWidth, (int)uiHeight);
            });
            break;
        case TerminalType::Script:
            terminal.Dialog->SetContentCallback([this, &terminal, &camera, uiWidth, uiHeight]() {
                RenderScriptTerminal(terminal, camera, (int)uiWidth, (int)uiHeight);
            });
            break;
        case TerminalType::Environment:
            terminal.Dialog->SetContentCallback([this, &terminal, &camera, uiWidth, uiHeight]() {
                RenderEnvironmentTerminal(terminal, camera, (int)uiWidth, (int)uiHeight);
            });
            break;
    }

    terminal.Dialog->Render(viewMatrix, projMatrix, (int)uiWidth, (int)uiHeight);
}

void TerminalHub::RenderTimeOfDayTerminal(Terminal& terminal, const Render::Camera& camera, int screenWidth, int screenHeight) {
    (void)terminal;
    (void)camera;
    (void)screenWidth;
    (void)screenHeight;

    ImGui::Text("Time of Day Control");
    ImGui::Separator();

    ImGui::Text("Hour: %.1f", m_TimeOfDay);
    if (ImGui::SliderFloat("Time", &m_TimeOfDay, 0.0f, 24.0f, "%.1f")) {
        if (m_Skybox) {
            m_Skybox->SetTimeOfDay(m_TimeOfDay);
            m_Skybox->Regenerate();
        }
    }

    ImGui::Spacing();
    ImGui::Text("Presets:");
    if (ImGui::Button("Dawn (6:00)")) {
        m_TimeOfDay = 6.0f;
        if (m_Skybox) {
            m_Skybox->SetTimeOfDay(6.0f);
            m_Skybox->Regenerate();
        }
    }
    if (ImGui::Button("Noon (12:00)")) {
        m_TimeOfDay = 12.0f;
        if (m_Skybox) {
            m_Skybox->SetTimeOfDay(12.0f);
            m_Skybox->Regenerate();
        }
    }
    if (ImGui::Button("Dusk (18:00)")) {
        m_TimeOfDay = 18.0f;
        if (m_Skybox) {
            m_Skybox->SetTimeOfDay(18.0f);
            m_Skybox->Regenerate();
        }
    }
    if (ImGui::Button("Night (0:00)")) {
        m_TimeOfDay = 0.0f;
        if (m_Skybox) {
            m_Skybox->SetTimeOfDay(0.0f);
            m_Skybox->Regenerate();
        }
    }
}

void TerminalHub::RenderWeatherTerminal(Terminal& terminal, const Render::Camera& camera, int screenWidth, int screenHeight) {
    (void)terminal;
    (void)camera;
    (void)screenWidth;
    (void)screenHeight;

    ImGui::Text("Weather Control");
    ImGui::Separator();
    ImGui::Text("Spawn Particle Systems:");

    if (ImGui::Button("Snow", ImVec2(150, 30))) {
        if (m_ParticleSpawnCallback) {
            m_ParticleSpawnCallback(Render::ParticlePresetType::Snow);
        }
    }
    if (ImGui::Button("Fire", ImVec2(150, 30))) {
        if (m_ParticleSpawnCallback) {
            m_ParticleSpawnCallback(Render::ParticlePresetType::Fire);
        }
    }
    if (ImGui::Button("Smoke", ImVec2(150, 30))) {
        if (m_ParticleSpawnCallback) {
            m_ParticleSpawnCallback(Render::ParticlePresetType::Smoke);
        }
    }
    if (ImGui::Button("Electricity", ImVec2(150, 30))) {
        if (m_ParticleSpawnCallback) {
            m_ParticleSpawnCallback(Render::ParticlePresetType::Electricity);
        }
    }
    if (ImGui::Button("Toxic Gas", ImVec2(150, 30))) {
        if (m_ParticleSpawnCallback) {
            m_ParticleSpawnCallback(Render::ParticlePresetType::ToxicGas);
        }
    }
}

void TerminalHub::RenderFluidTerminal(Terminal& terminal, const Render::Camera& camera, int screenWidth, int screenHeight) {
    (void)terminal;
    (void)camera;
    (void)screenWidth;
    (void)screenHeight;

    ImGui::Text("Fluid Container Control");
    ImGui::Separator();
    ImGui::Text("Spawn a fluid container to demonstrate");
    ImGui::Text("buoyancy physics.");

    ImGui::Spacing();
    if (ImGui::Button("Spawn Fluid Container", ImVec2(200, 40))) {
        if (m_FluidSpawnCallback) {
            Math::Vec3 spawnPos = camera.Position + camera.Front * 5.0f;
            m_FluidSpawnCallback(spawnPos);
        }
    }
}

void TerminalHub::RenderScriptTerminal(Terminal& terminal, const Render::Camera& camera, int screenWidth, int screenHeight) {
    (void)terminal;
    (void)camera;
    (void)screenWidth;
    (void)screenHeight;

    ImGui::Text("Moonwalk Script Terminal");
    ImGui::Separator();

    ImGui::Text("Enter Moonwalk script:");
    // Use a char buffer for ImGui input (static to persist across frames)
    static char scriptBuffer[4096] = "";
    static bool bufferInitialized = false;

    // Initialize buffer from m_ScriptInput if not already done or if input changed externally
    if (!bufferInitialized || m_ScriptInput != std::string(scriptBuffer)) {
        std::strncpy(scriptBuffer, m_ScriptInput.c_str(), sizeof(scriptBuffer) - 1);
        scriptBuffer[sizeof(scriptBuffer) - 1] = '\0';
        bufferInitialized = true;
    }

    if (ImGui::InputTextMultiline("##ScriptInput", scriptBuffer, sizeof(scriptBuffer), ImVec2(480, 250))) {
        m_ScriptInput = scriptBuffer;
    }

    ImGui::Spacing();
    if (ImGui::Button("Compile & Execute", ImVec2(200, 30))) {
        if (m_ScriptExecuteCallback) {
            std::string error;
            bool success = m_ScriptExecuteCallback(m_ScriptInput, error);
            if (!success) {
                m_ScriptError = error;
            } else {
                m_ScriptError = "Script executed successfully!";
            }
        }
    }

    if (!m_ScriptError.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Output:");
        ImGui::TextWrapped("%s", m_ScriptError.c_str());
    }
}

void TerminalHub::RenderEnvironmentTerminal(Terminal& terminal, const Render::Camera& camera, int screenWidth, int screenHeight) {
    (void)terminal;
    (void)camera;
    (void)screenWidth;
    (void)screenHeight;

    ImGui::Text("Environment Control");
    ImGui::Separator();

    ImGui::Text("Skybox Presets:");
    const char* presets[] = { "Dawn", "Noon", "Dusk", "Night", "Overcast", "Clear" };
    if (ImGui::Combo("Preset", &m_CurrentSkyPreset, presets, 6)) {
        if (m_Skybox) {
            Render::SkyPreset preset = static_cast<Render::SkyPreset>(m_CurrentSkyPreset);
            m_Skybox->SetPreset(preset);
            m_Skybox->Regenerate();
        }
    }
}

bool TerminalHub::IsNearTerminal(const Math::Vec3& playerPos, const Math::Vec3& terminalPos, float distance) const {
    Math::Vec3 diff = playerPos - terminalPos;
    return diff.Magnitude() < distance;
}

std::vector<Math::Vec3> TerminalHub::GetTerminalPositions() const {
    std::vector<Math::Vec3> positions;
    for (const auto& terminal : m_Terminals) {
        positions.push_back(terminal.Position);
    }
    return positions;
}

} // namespace Solstice::PhysicsPlayground

