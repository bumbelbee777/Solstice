#include "UIManager.hxx"
#include "ObjectSpawner.hxx"
#include <UI/Widgets.hxx>
#include <imgui.h>
#include <string>

namespace Solstice::PhysicsPlayground {

UIManager::UIManager(ObjectSpawner& objectSpawner, Render::Camera& camera)
    : m_ObjectSpawner(objectSpawner), m_Camera(camera) {
}

void UIManager::Render() {
    RenderMainMenu();
    RenderAddObjectDialog();
    RenderDebugInfo();
}

void UIManager::RenderMainMenu() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Objects")) {
            if (ImGui::MenuItem("Add Object...")) {
                m_ShowAddObjectDialog = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void UIManager::RenderAddObjectDialog() {
    if (m_ShowAddObjectDialog) {
        ImGui::OpenPopup("Add Object");
        m_ShowAddObjectDialog = false;
    }

    if (ImGui::BeginPopupModal("Add Object", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Select object type to add:");
        ImGui::Separator();
        ImGui::Spacing();

        // Grid of buttons for object types
        const char* objectNames[] = {
            "Sphere", "Cylinder", "Tetrahedron", "Cube",
            "Pyramid", "Torus", "Icosphere"
        };

        ObjectType objectTypes[] = {
            ObjectType::Sphere, ObjectType::Cylinder, ObjectType::Tetrahedron, ObjectType::Cube,
            ObjectType::Pyramid, ObjectType::Torus, ObjectType::Icosphere
        };

        const int itemsPerRow = 3;
        for (size_t i = 0; i < 7; ++i) {
            if (i > 0 && i % itemsPerRow != 0) {
                ImGui::SameLine();
            }

            if (ImGui::Button(objectNames[i], ImVec2(100, 30))) {
                // Trigger spawn callback
                if (m_SpawnCallback) {
                    m_SpawnCallback(objectTypes[i]);
                } else {
                    // Fallback: spawn directly
                    Math::Vec3 spawnPos = m_Camera.Position + m_Camera.Front * 5.0f;
                    m_ObjectSpawner.SpawnObject(objectTypes[i], spawnPos, Math::Quaternion());
                }
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void UIManager::RenderDebugInfo() {
    ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 200), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Debug Info", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Solstice Engine - Physics Playground");
        ImGui::Separator();

        ImGui::Text("FPS: %.1f", m_FPS);
        ImGui::Text("Frame Time: %.2f ms", m_FrameTime);
        ImGui::Separator();

        ImGui::Text("Camera Position:");
        ImGui::Text("  X: %.2f  Y: %.2f  Z: %.2f", m_Camera.Position.x, m_Camera.Position.y, m_Camera.Position.z);
        ImGui::Text("Camera Rotation:");
        ImGui::Text("  Yaw: %.1f  Pitch: %.1f", m_Camera.Yaw, m_Camera.Pitch);
        ImGui::Separator();

        ImGui::Text("Physics Objects: %zu", m_ObjectCount);
        if (m_ObjectGrabbed) {
            ImGui::Text("Object Grabbed: Yes");
        } else {
            ImGui::Text("Object Grabbed: No");
        }
        ImGui::Separator();

        ImGui::Text("Controls:");
        ImGui::BulletText("E - Pick up/Drop object");
        ImGui::BulletText("Right Mouse - Lock camera");
        ImGui::BulletText("WASD - Move camera");
        ImGui::BulletText("F - Toggle fullscreen/maximized");
        ImGui::BulletText("Left Click - Select object");
    }
    ImGui::End();
}

} // namespace Solstice::PhysicsPlayground
