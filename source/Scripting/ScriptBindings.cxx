#include "ScriptBindings.hxx"
#include "BytecodeVM.hxx"
#include <UI/Widgets.hxx>
#include <Render/Scene.hxx>

#include <iostream>
#include <string>

namespace Solstice::Scripting {

    using namespace Solstice::Math;

    void RegisterScriptBindings(BytecodeVM& vm, Solstice::Render::Scene* scene) {
        // Console
        vm.RegisterNative("print", [](const std::vector<Value>& args) -> Value {
            for (const auto& arg : args) {
                if (std::holds_alternative<int64_t>(arg)) std::cout << std::get<int64_t>(arg);
                else if (std::holds_alternative<double>(arg)) std::cout << std::get<double>(arg);
                else if (std::holds_alternative<std::string>(arg)) std::cout << std::get<std::string>(arg);
                std::cout << " ";
            }
            std::cout << "\n";
            return (int64_t)0;
        });

        // UI
        vm.RegisterNative("UI.BeginWindow", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<std::string>(args[0])) {
                UI::Widgets::BeginWindow(std::get<std::string>(args[0]));
            }
            return (int64_t)0;
        });

        vm.RegisterNative("UI.EndWindow", [](const std::vector<Value>& args) -> Value {
            UI::Widgets::EndWindow();
            return (int64_t)0;
        });

        vm.RegisterNative("UI.Text", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<std::string>(args[0])) {
                UI::Widgets::Text(std::get<std::string>(args[0]));
            }
            return (int64_t)0;
        });

        vm.RegisterNative("UI.Button", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<std::string>(args[0])) {
                bool clicked = UI::Widgets::Button(std::get<std::string>(args[0]));
                return (int64_t)(clicked ? 1 : 0);
            }
            return (int64_t)0;
        });

        // Render
        if (scene) {
            vm.RegisterNative("Render.SetTransform", [scene](const std::vector<Value>& args) -> Value {
                // Id, x, y, z
                if (args.size() >= 4 && std::holds_alternative<int64_t>(args[0])) {
                    int64_t id = std::get<int64_t>(args[0]);
                    double x = 0, y = 0, z = 0;
                    
                    if (std::holds_alternative<int64_t>(args[1])) x = (double)std::get<int64_t>(args[1]);
                    else if (std::holds_alternative<double>(args[1])) x = std::get<double>(args[1]);

                    if (std::holds_alternative<int64_t>(args[2])) y = (double)std::get<int64_t>(args[2]);
                    else if (std::holds_alternative<double>(args[2])) y = std::get<double>(args[2]);

                    if (std::holds_alternative<int64_t>(args[3])) z = (double)std::get<int64_t>(args[3]);
                    else if (std::holds_alternative<double>(args[3])) z = std::get<double>(args[3]);

                    scene->SetTransform((Render::SceneObjectID)id, Vec3((float)x, (float)y, (float)z), Quaternion(), Vec3(1,1,1));
                }
                return (int64_t)0;
            });
        }
    }
}
