#include "ScriptBindings.hxx"
#include "../VM/BytecodeVM.hxx"
#include "../../UI/Widgets/Widgets.hxx"
#include <Render/Scene/Scene.hxx>
#include <Render/Scene/Camera.hxx>
#include "../../Entity/Registry.hxx"
#include "../../Entity/Transform.hxx"
#include "../../Entity/Name.hxx"
#include "../../Entity/Kind.hxx"
#include "../../Entity/Components/FacialComponents.hxx"
#include "../../Physics/Integration/PhysicsSystem.hxx"
#include "../../Physics/Dynamics/RigidBody.hxx"
#include "../../Physics/Dynamics/Vehicle.hxx"
#include "../../Physics/Dynamics/SoftBody.hxx"
#include "../../Arzachel/AnimationClip.hxx"
#include "../../Arzachel/Generator.hxx"
#include "../../Arzachel/FacialAnimation.hxx"
#include "../../Arzachel/TextLipSync.hxx"
#include "../../UI/Motion/MotionGraphics.hxx"
#include "../../Core/Audio/Audio.hxx"
#include "../../Core/Profiling/Profiler.hxx"
#include "../../Core/Relic/Relic.hxx"
#include <imgui.h>

#include <iostream>
#include <string>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cctype>

namespace Solstice::Scripting {

    using namespace Solstice::Math;
    namespace {
        ScriptInputHooks g_InputHooks{};
    }

    SOLSTICE_API void SetScriptInputHooks(const ScriptInputHooks& hooks) {
        g_InputHooks = hooks;
    }

    // Helper to extract float from Value
    float GetFloat(const Value& v) {
        if (std::holds_alternative<double>(v)) return (float)std::get<double>(v);
        if (std::holds_alternative<int64_t>(v)) return (float)std::get<int64_t>(v);
        return 0.0f;
    }

    // Helper to extract int from Value
    int64_t GetInt(const Value& v) {
        if (std::holds_alternative<int64_t>(v)) return std::get<int64_t>(v);
        if (std::holds_alternative<double>(v)) return (int64_t)std::get<double>(v);
        return 0;
    }

    // Helper to extract string from Value
    std::string GetString(const Value& v) {
        if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
        if (std::holds_alternative<int64_t>(v)) return std::to_string(std::get<int64_t>(v));
        if (std::holds_alternative<double>(v)) return std::to_string(std::get<double>(v));
        return "";
    }

    SOLSTICE_API void RegisterScriptBindings(
        BytecodeVM& vm,
        ECS::Registry* registry,
        Render::Scene* scene,
        Physics::PhysicsSystem* physicsSystem,
        Render::Camera* camera
    ) {
        // Set registry in VM
        if (registry) {
            vm.SetRegistry(registry);
        }

        // Console
        vm.RegisterNative("Print", [](const std::vector<Value>& args) -> Value {
            for (const auto& arg : args) {
                if (std::holds_alternative<int64_t>(arg)) std::cout << std::get<int64_t>(arg);
                else if (std::holds_alternative<double>(arg)) std::cout << std::get<double>(arg);
                else if (std::holds_alternative<std::string>(arg)) std::cout << std::get<std::string>(arg);
                else if (std::holds_alternative<Vec2>(arg)) {
                    const auto& v = std::get<Vec2>(arg);
                    std::cout << "Vec2(" << v.x << ", " << v.y << ")";
                }
                else if (std::holds_alternative<Vec3>(arg)) {
                    const auto& v = std::get<Vec3>(arg);
                    std::cout << "Vec3(" << v.x << ", " << v.y << ", " << v.z << ")";
                }
                else if (std::holds_alternative<Vec4>(arg)) {
                    const auto& v = std::get<Vec4>(arg);
                    std::cout << "Vec4(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
                }
                else if (std::holds_alternative<Quaternion>(arg)) {
                    const auto& q = std::get<Quaternion>(arg);
                    std::cout << "Quaternion(" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")";
                }
                else if (std::holds_alternative<std::shared_ptr<Array>>(arg)) {
                    const auto& arr = std::get<std::shared_ptr<Array>>(arg);
                    std::cout << "Array[" << arr->Length() << "]";
                }
                else if (std::holds_alternative<std::shared_ptr<Dictionary>>(arg)) {
                    const auto& dict = std::get<std::shared_ptr<Dictionary>>(arg);
                    std::cout << "Dictionary{" << dict->Size() << "}";
                }
                else if (std::holds_alternative<std::shared_ptr<Set>>(arg)) {
                    const auto& s = std::get<std::shared_ptr<Set>>(arg);
                    std::cout << "Set{" << s->Size() << "}";
                }
                std::cout << " ";
            }
            std::cout << "\n";
            return (int64_t)0;
        });

        // ========== String Operations ==========
        vm.RegisterNative("String.ToUpper", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0) {
                std::string str = GetString(args[0]);
                std::transform(str.begin(), str.end(), str.begin(), ::toupper);
                return str;
            }
            return std::string("");
        });

        vm.RegisterNative("String.ToLower", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0) {
                std::string str = GetString(args[0]);
                std::transform(str.begin(), str.end(), str.begin(), ::tolower);
                return str;
            }
            return std::string("");
        });

        vm.RegisterNative("String.Find", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2) {
                std::string str = GetString(args[0]);
                std::string substring = GetString(args[1]);
                size_t pos = str.find(substring);
                return (int64_t)(pos != std::string::npos ? (int64_t)pos : -1);
            }
            return (int64_t)-1;
        });

        vm.RegisterNative("String.Replace", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 3) {
                std::string str = GetString(args[0]);
                std::string oldStr = GetString(args[1]);
                std::string newStr = GetString(args[2]);
                size_t pos = 0;
                while ((pos = str.find(oldStr, pos)) != std::string::npos) {
                    str.replace(pos, oldStr.length(), newStr);
                    pos += newStr.length();
                }
                return str;
            }
            return std::string("");
        });

        vm.RegisterNative("String.Split", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2) {
                std::string str = GetString(args[0]);
                std::string delimiter = GetString(args[1]);
                auto result = std::make_shared<Array>();
                
                if (delimiter.empty()) {
                    // Split by characters
                    for (char c : str) {
                        result->Push(std::string(1, c));
                    }
                } else {
                    size_t start = 0;
                    size_t end = str.find(delimiter);
                    while (end != std::string::npos) {
                        result->Push(str.substr(start, end - start));
                        start = end + delimiter.length();
                        end = str.find(delimiter, start);
                    }
                    result->Push(str.substr(start));
                }
                return result;
            }
            return std::make_shared<Array>();
        });

        vm.RegisterNative("String.Trim", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0) {
                std::string str = GetString(args[0]);
                // Trim leading whitespace
                str.erase(0, str.find_first_not_of(" \t\n\r\f\v"));
                // Trim trailing whitespace
                str.erase(str.find_last_not_of(" \t\n\r\f\v") + 1);
                return str;
            }
            return std::string("");
        });

        vm.RegisterNative("String.StartsWith", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2) {
                std::string str = GetString(args[0]);
                std::string prefix = GetString(args[1]);
                return (int64_t)(str.length() >= prefix.length() && str.substr(0, prefix.length()) == prefix ? 1 : 0);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("String.EndsWith", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2) {
                std::string str = GetString(args[0]);
                std::string suffix = GetString(args[1]);
                return (int64_t)(str.length() >= suffix.length() && str.substr(str.length() - suffix.length()) == suffix ? 1 : 0);
            }
            return (int64_t)0;
        });

        // ========== Math Types: Vec2 ==========
        vm.RegisterNative("Vec2.New", [](const std::vector<Value>& args) -> Value {
            float x = args.size() > 0 ? GetFloat(args[0]) : 0.0f;
            float y = args.size() > 1 ? GetFloat(args[1]) : 0.0f;
            return Vec2(x, y);
        });

        vm.RegisterNative("Vec2.X", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Vec2>(args[0])) {
                return (double)std::get<Vec2>(args[0]).x;
            }
            return (double)0.0;
        });

        vm.RegisterNative("Vec2.Y", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Vec2>(args[0])) {
                return (double)std::get<Vec2>(args[0]).y;
            }
            return (double)0.0;
        });

        vm.RegisterNative("Vec2.Magnitude", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Vec2>(args[0])) {
                Vec2 v = std::get<Vec2>(args[0]); // Non-const copy
                return (double)v.Magnitude();
            }
            return (double)0.0;
        });

        vm.RegisterNative("Vec2.Dot", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<Vec2>(args[0]) && std::holds_alternative<Vec2>(args[1])) {
                return (double)std::get<Vec2>(args[0]).Dot(std::get<Vec2>(args[1]));
            }
            return (double)0.0;
        });

        vm.RegisterNative("Vec2.Normalized", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Vec2>(args[0])) {
                Vec2 v = std::get<Vec2>(args[0]); // Non-const copy
                return v.Normalized();
            }
            return Vec2();
        });

        vm.RegisterNative("Vec2.Add", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<Vec2>(args[0]) && std::holds_alternative<Vec2>(args[1])) {
                return std::get<Vec2>(args[0]) + std::get<Vec2>(args[1]);
            }
            return Vec2();
        });

        vm.RegisterNative("Vec2.Subtract", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<Vec2>(args[0]) && std::holds_alternative<Vec2>(args[1])) {
                return std::get<Vec2>(args[0]) - std::get<Vec2>(args[1]);
            }
            return Vec2();
        });

        vm.RegisterNative("Vec2.Multiply", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<Vec2>(args[0])) {
                float scalar = GetFloat(args[1]);
                return std::get<Vec2>(args[0]) * scalar;
            }
            return Vec2();
        });

        // ========== Math Types: Vec3 ==========
        vm.RegisterNative("Vec3.New", [](const std::vector<Value>& args) -> Value {
            float x = args.size() > 0 ? GetFloat(args[0]) : 0.0f;
            float y = args.size() > 1 ? GetFloat(args[1]) : 0.0f;
            float z = args.size() > 2 ? GetFloat(args[2]) : 0.0f;
            return Vec3(x, y, z);
        });

        vm.RegisterNative("Vec3.X", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Vec3>(args[0])) {
                return (double)std::get<Vec3>(args[0]).x;
            }
            return (double)0.0;
        });

        vm.RegisterNative("Vec3.Y", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Vec3>(args[0])) {
                return (double)std::get<Vec3>(args[0]).y;
            }
            return (double)0.0;
        });

        vm.RegisterNative("Vec3.Z", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Vec3>(args[0])) {
                return (double)std::get<Vec3>(args[0]).z;
            }
            return (double)0.0;
        });

        vm.RegisterNative("Vec3.Magnitude", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Vec3>(args[0])) {
                Vec3 v = std::get<Vec3>(args[0]); // Non-const copy
                return (double)v.Magnitude();
            }
            return (double)0.0;
        });

        vm.RegisterNative("Vec3.Dot", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<Vec3>(args[0]) && std::holds_alternative<Vec3>(args[1])) {
                return (double)std::get<Vec3>(args[0]).Dot(std::get<Vec3>(args[1]));
            }
            return (double)0.0;
        });

        vm.RegisterNative("Vec3.Cross", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<Vec3>(args[0]) && std::holds_alternative<Vec3>(args[1])) {
                return std::get<Vec3>(args[0]).Cross(std::get<Vec3>(args[1]));
            }
            return Vec3();
        });

        vm.RegisterNative("Vec3.Normalized", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Vec3>(args[0])) {
                Vec3 v = std::get<Vec3>(args[0]); // Non-const copy
                return v.Normalized();
            }
            return Vec3();
        });

        vm.RegisterNative("Vec3.Distance", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<Vec3>(args[0]) && std::holds_alternative<Vec3>(args[1])) {
                return (double)std::get<Vec3>(args[0]).Distance(std::get<Vec3>(args[1]));
            }
            return (double)0.0;
        });

        vm.RegisterNative("Vec3.Add", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<Vec3>(args[0]) && std::holds_alternative<Vec3>(args[1])) {
                return std::get<Vec3>(args[0]) + std::get<Vec3>(args[1]);
            }
            return Vec3();
        });

        vm.RegisterNative("Vec3.Subtract", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<Vec3>(args[0]) && std::holds_alternative<Vec3>(args[1])) {
                return std::get<Vec3>(args[0]) - std::get<Vec3>(args[1]);
            }
            return Vec3();
        });

        vm.RegisterNative("Vec3.Multiply", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<Vec3>(args[0])) {
                float scalar = GetFloat(args[1]);
                return std::get<Vec3>(args[0]) * scalar;
            }
            return Vec3();
        });

        // ========== Math Types: Vec4 ==========
        vm.RegisterNative("Vec4.New", [](const std::vector<Value>& args) -> Value {
            float x = args.size() > 0 ? GetFloat(args[0]) : 0.0f;
            float y = args.size() > 1 ? GetFloat(args[1]) : 0.0f;
            float z = args.size() > 2 ? GetFloat(args[2]) : 0.0f;
            float w = args.size() > 3 ? GetFloat(args[3]) : 0.0f;
            return Vec4(x, y, z, w);
        });

        vm.RegisterNative("Vec4.Magnitude", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Vec4>(args[0])) {
                Vec4 v = std::get<Vec4>(args[0]); // Non-const copy
                return (double)v.Magnitude();
            }
            return (double)0.0;
        });

        vm.RegisterNative("Vec4.Dot", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<Vec4>(args[0]) && std::holds_alternative<Vec4>(args[1])) {
                return (double)std::get<Vec4>(args[0]).Dot(std::get<Vec4>(args[1]));
            }
            return (double)0.0;
        });

        vm.RegisterNative("Vec4.Normalized", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Vec4>(args[0])) {
                Vec4 v = std::get<Vec4>(args[0]); // Non-const copy
                return v.Normalized();
            }
            return Vec4();
        });

        // ========== Math Types: Mat2 ==========
        vm.RegisterNative("Mat2.New", [](const std::vector<Value>& args) -> Value {
            return Matrix2();
        });

        vm.RegisterNative("Mat2.Identity", [](const std::vector<Value>& args) -> Value {
            return Matrix2::Identity();
        });

        vm.RegisterNative("Mat2.Multiply", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<Matrix2>(args[0]) && std::holds_alternative<Matrix2>(args[1])) {
                return std::get<Matrix2>(args[0]) * std::get<Matrix2>(args[1]);
            }
            return Matrix2();
        });

        vm.RegisterNative("Mat2.Transpose", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Matrix2>(args[0])) {
                return std::get<Matrix2>(args[0]).Transposed();
            }
            return Matrix2();
        });

        vm.RegisterNative("Mat2.Determinant", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Matrix2>(args[0])) {
                return (double)std::get<Matrix2>(args[0]).Determinant();
            }
            return (double)0.0;
        });

        // ========== Math Types: Mat3 ==========
        vm.RegisterNative("Mat3.New", [](const std::vector<Value>& args) -> Value {
            return Matrix3();
        });

        vm.RegisterNative("Mat3.Identity", [](const std::vector<Value>& args) -> Value {
            return Matrix3::Identity();
        });

        vm.RegisterNative("Mat3.Multiply", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<Matrix3>(args[0]) && std::holds_alternative<Matrix3>(args[1])) {
                return std::get<Matrix3>(args[0]) * std::get<Matrix3>(args[1]);
            }
            return Matrix3();
        });

        vm.RegisterNative("Mat3.Transpose", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Matrix3>(args[0])) {
                return std::get<Matrix3>(args[0]).Transposed();
            }
            return Matrix3();
        });

        // ========== Math Types: Mat4 ==========
        vm.RegisterNative("Mat4.New", [](const std::vector<Value>& args) -> Value {
            return Matrix4();
        });

        vm.RegisterNative("Mat4.Identity", [](const std::vector<Value>& args) -> Value {
            return Matrix4::Identity();
        });

        vm.RegisterNative("Mat4.Multiply", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<Matrix4>(args[0]) && std::holds_alternative<Matrix4>(args[1])) {
                return std::get<Matrix4>(args[0]) * std::get<Matrix4>(args[1]);
            }
            return Matrix4();
        });

        vm.RegisterNative("Mat4.Transpose", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Matrix4>(args[0])) {
                return std::get<Matrix4>(args[0]).Transposed();
            }
            return Matrix4();
        });

        vm.RegisterNative("Mat4.Translation", [](const std::vector<Value>& args) -> Value {
            float x = args.size() > 0 ? GetFloat(args[0]) : 0.0f;
            float y = args.size() > 1 ? GetFloat(args[1]) : 0.0f;
            float z = args.size() > 2 ? GetFloat(args[2]) : 0.0f;
            return Matrix4::Translation(Vec3(x, y, z));
        });

        vm.RegisterNative("Mat4.RotationX", [](const std::vector<Value>& args) -> Value {
            float angle = args.size() > 0 ? GetFloat(args[0]) : 0.0f;
            return Matrix4::RotationX(angle);
        });

        vm.RegisterNative("Mat4.RotationY", [](const std::vector<Value>& args) -> Value {
            float angle = args.size() > 0 ? GetFloat(args[0]) : 0.0f;
            return Matrix4::RotationY(angle);
        });

        vm.RegisterNative("Mat4.RotationZ", [](const std::vector<Value>& args) -> Value {
            float angle = args.size() > 0 ? GetFloat(args[0]) : 0.0f;
            return Matrix4::RotationZ(angle);
        });

        vm.RegisterNative("Mat4.Scale", [](const std::vector<Value>& args) -> Value {
            float x = args.size() > 0 ? GetFloat(args[0]) : 1.0f;
            float y = args.size() > 1 ? GetFloat(args[1]) : 1.0f;
            float z = args.size() > 2 ? GetFloat(args[2]) : 1.0f;
            return Matrix4::Scale(Vec3(x, y, z));
        });

        vm.RegisterNative("Mat4.Perspective", [](const std::vector<Value>& args) -> Value {
            float fov = args.size() > 0 ? GetFloat(args[0]) : 1.0f;
            float aspect = args.size() > 1 ? GetFloat(args[1]) : 1.0f;
            float nearPlane = args.size() > 2 ? GetFloat(args[2]) : 0.1f;
            float farPlane = args.size() > 3 ? GetFloat(args[3]) : 100.0f;
            return Matrix4::Perspective(fov, aspect, nearPlane, farPlane);
        });

        vm.RegisterNative("Mat4.LookAt", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 9) {
                Vec3 eye(GetFloat(args[0]), GetFloat(args[1]), GetFloat(args[2]));
                Vec3 target(GetFloat(args[3]), GetFloat(args[4]), GetFloat(args[5]));
                Vec3 up(GetFloat(args[6]), GetFloat(args[7]), GetFloat(args[8]));
                return Matrix4::LookAt(eye, target, up);
            }
            return Matrix4();
        });

        // ========== Math Types: Quaternion ==========
        vm.RegisterNative("Quaternion.New", [](const std::vector<Value>& args) -> Value {
            float w = args.size() > 0 ? GetFloat(args[0]) : 1.0f;
            float x = args.size() > 1 ? GetFloat(args[1]) : 0.0f;
            float y = args.size() > 2 ? GetFloat(args[2]) : 0.0f;
            float z = args.size() > 3 ? GetFloat(args[3]) : 0.0f;
            return Quaternion(w, x, y, z);
        });

        vm.RegisterNative("Quaternion.Multiply", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<Quaternion>(args[0]) && std::holds_alternative<Quaternion>(args[1])) {
                return std::get<Quaternion>(args[0]) * std::get<Quaternion>(args[1]);
            }
            return Quaternion();
        });

        vm.RegisterNative("Quaternion.Normalized", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Quaternion>(args[0])) {
                return std::get<Quaternion>(args[0]).Normalized();
            }
            return Quaternion();
        });

        vm.RegisterNative("Quaternion.Conjugate", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Quaternion>(args[0])) {
                return std::get<Quaternion>(args[0]).Conjugate();
            }
            return Quaternion();
        });

        vm.RegisterNative("Quaternion.Lerp", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 3 && std::holds_alternative<Quaternion>(args[0]) && std::holds_alternative<Quaternion>(args[1])) {
                float t = GetFloat(args[2]);
                return Quaternion::Lerp(std::get<Quaternion>(args[0]), std::get<Quaternion>(args[1]), t);
            }
            return Quaternion();
        });

        vm.RegisterNative("Quaternion.Slerp", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 3 && std::holds_alternative<Quaternion>(args[0]) && std::holds_alternative<Quaternion>(args[1])) {
                float t = GetFloat(args[2]);
                return Quaternion::Slerp(std::get<Quaternion>(args[0]), std::get<Quaternion>(args[1]), t);
            }
            return Quaternion();
        });

        vm.RegisterNative("Quaternion.ToMatrix", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<Quaternion>(args[0])) {
                return std::get<Quaternion>(args[0]).ToMatrix();
            }
            return Matrix4();
        });

        // ========== ECS Functions ==========
        if (registry) {
            vm.RegisterNative("ECS.Create", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (int64_t)0;
                return (int64_t)registry->Create();
            });

            vm.RegisterNative("ECS.Destroy", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (int64_t)0;
                if (args.size() > 0) {
                    ECS::EntityId id = (ECS::EntityId)GetInt(args[0]);
                    registry->Destroy(id);
                }
                return (int64_t)0;
            });

            vm.RegisterNative("ECS.Valid", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (int64_t)0;
                if (args.size() > 0) {
                    ECS::EntityId id = (ECS::EntityId)GetInt(args[0]);
                    return (int64_t)(registry->Valid(id) ? 1 : 0);
                }
                return (int64_t)0;
            });

            vm.RegisterNative("ECS.AddTransform", [registry](const std::vector<Value>& args) -> Value {
                if (args.size() >= 1) {
                    ECS::EntityId id = (ECS::EntityId)GetInt(args[0]);
                    ECS::Transform t;
                    if (args.size() >= 4) {
                        t.Position = Vec3(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3]));
                    }
                    if (args.size() >= 7) {
                        t.Scale = Vec3(GetFloat(args[4]), GetFloat(args[5]), GetFloat(args[6]));
                    }
                    registry->Add<ECS::Transform>(id, t);
                }
                return (int64_t)0;
            });

            vm.RegisterNative("ECS.AddName", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (int64_t)0;
                if (args.size() >= 2) {
                    ECS::EntityId id = (ECS::EntityId)GetInt(args[0]);
                    std::string name = GetString(args[1]);
                    registry->Add<ECS::Name>(id, ECS::Name{name});
                }
                return (int64_t)0;
            });

            vm.RegisterNative("ECS.GetTransform", [registry](const std::vector<Value>& args) -> Value {
                if (!registry || args.empty()) return Vec3();
                if (const ECS::Transform* t = registry->TryGet<ECS::Transform>((ECS::EntityId)GetInt(args[0]))) {
                    return t->Position; // Return Vec3
                }
                return Vec3();
            });

            vm.RegisterNative("ECS.GetName", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return std::string("");
                if (args.size() > 0) {
                    if (const ECS::Name* n = registry->TryGet<ECS::Name>((ECS::EntityId)GetInt(args[0]))) {
                        return n->Value;
                    }
                }
                return std::string("");
            });

            vm.RegisterNative("ECS.HasTransform", [registry](const std::vector<Value>& args) -> Value {
                if (args.size() > 0) {
                    return (int64_t)(registry->Has<ECS::Transform>((ECS::EntityId)GetInt(args[0])) ? 1 : 0);
                }
                return (int64_t)0;
            });

            vm.RegisterNative("ECS.HasName", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (int64_t)0;
                if (args.size() > 0) {
                    return (int64_t)(registry->Has<ECS::Name>((ECS::EntityId)GetInt(args[0])) ? 1 : 0);
                }
                return (int64_t)0;
            });

            vm.RegisterNative("ECS.RemoveTransform", [registry](const std::vector<Value>& args) -> Value {
                if (args.size() > 0) {
                    registry->Remove<ECS::Transform>((ECS::EntityId)GetInt(args[0]));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("ECS.RemoveName", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (int64_t)0;
                if (args.size() > 0) {
                    registry->Remove<ECS::Name>((ECS::EntityId)GetInt(args[0]));
                }
                return (int64_t)0;
            });
        }

        // ========== System Registration ==========
        vm.RegisterNative("System.Register", [&vm](const std::vector<Value>& args) -> Value {
            // System.Register(name, functionName, componentName1, componentName2, ...)
            // Resolve function address from function name
            if (args.size() >= 2) {
                std::string name = GetString(args[0]);
                std::string funcName = GetString(args[1]);
                BytecodeVM::SystemInfo info;
                info.Name = name;
                
                // Try to resolve function address from exports
                size_t functionAddress = 0;
                if (vm.GetProgram().Exports.find(funcName) != vm.GetProgram().Exports.end()) {
                    functionAddress = vm.GetProgram().Exports.at(funcName);
                } else {
                    // Try module.function format
                    size_t dotPos = funcName.find('.');
                    if (dotPos != std::string::npos) {
                        std::string modName = funcName.substr(0, dotPos);
                        std::string modFuncName = funcName.substr(dotPos + 1);
                        if (vm.HasModule(modName)) {
                            const auto& mod = vm.GetModule(modName);
                            if (mod.Exports.find(modFuncName) != mod.Exports.end()) {
                                functionAddress = mod.Exports.at(modFuncName);
                            }
                        }
                    }
                }
                
                info.FunctionAddress = functionAddress;
                for (size_t i = 2; i < args.size(); ++i) {
                    info.ComponentNames.push_back(GetString(args[i]));
                }
                vm.RegisterSystem(info);
            }
            return (int64_t)0;
        });
        
        // ========== Time Functions ==========
        vm.RegisterNative("Time.GetDeltaTime", [](const std::vector<Value>& args) -> Value {
            // Get delta time from Profiler
            auto stats = Core::Profiler::Instance().GetLastFrameStats();
            return (double)stats.FrameTime;
        });
        
        vm.RegisterNative("Time.GetGameTime", [](const std::vector<Value>& args) -> Value {
            // Game time would need to be passed from GameBase
            // For now, use frame time as approximation
            auto stats = Core::Profiler::Instance().GetLastFrameStats();
            return (double)stats.FrameTime;
        });
        
        vm.RegisterNative("Time.GetFPS", [](const std::vector<Value>& args) -> Value {
            auto stats = Core::Profiler::Instance().GetLastFrameStats();
            return (double)stats.FPS;
        });

        // ========== RELIC Assets (prefetch, is loaded, unload) ==========
        vm.RegisterNative("Assets.Prefetch", [](const std::vector<Value>& args) -> Value {
            auto* svc = Core::Relic::GetAssetService();
            if (svc && args.size() > 0) {
                uint32_t clusterId = static_cast<uint32_t>(GetInt(args[0]));
                svc->PrefetchCluster(clusterId);
            }
            return (int64_t)0;
        });
        vm.RegisterNative("Assets.IsLoaded", [](const std::vector<Value>& args) -> Value {
            auto* svc = Core::Relic::GetAssetService();
            if (svc && args.size() > 0) {
                uint64_t hash = static_cast<uint64_t>(GetInt(args[0]));
                return (int64_t)(svc->IsLoaded(hash) ? 1 : 0);
            }
            return (int64_t)0;
        });
        vm.RegisterNative("Assets.Unload", [](const std::vector<Value>& args) -> Value {
            auto* svc = Core::Relic::GetAssetService();
            if (svc && args.size() > 0) {
                uint32_t clusterId = static_cast<uint32_t>(GetInt(args[0]));
                svc->UnloadCluster(clusterId);
            }
            return (int64_t)0;
        });

        // ========== Events / Callbacks ==========
        vm.RegisterNative("Events.On", [&vm](const std::vector<Value>& args) -> Value {
            if (args.size() < 2 || !std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<ScriptFunc>(args[1]))
                return (int64_t)0;
            vm.RegisterEventHandler(std::get<std::string>(args[0]), std::get<ScriptFunc>(args[1]));
            return (int64_t)0;
        });
        vm.RegisterNative("Events.Emit", [&vm](const std::vector<Value>& args) -> Value {
            if (args.size() < 1 || !std::holds_alternative<std::string>(args[0]))
                return (int64_t)0;
            std::string name = std::get<std::string>(args[0]);
            std::vector<Value> payload(args.begin() + 1, args.end());
            vm.EmitEvent(name, payload);
            return (int64_t)0;
        });
        vm.RegisterNative("Events.HandlerCount", [&vm](const std::vector<Value>& args) -> Value {
            if (args.empty() || !std::holds_alternative<std::string>(args[0])) {
                return (int64_t)0;
            }
            return static_cast<int64_t>(vm.GetEventHandlerCount(std::get<std::string>(args[0])));
        });
        vm.RegisterNative("Events.Clear", [&vm](const std::vector<Value>& args) -> Value {
            if (!args.empty() && std::holds_alternative<std::string>(args[0])) {
                vm.ClearEventHandlers(std::get<std::string>(args[0]));
            }
            return (int64_t)0;
        });
        vm.RegisterNative("Events.ClearAll", [&vm](const std::vector<Value>& args) -> Value {
            (void)args;
            vm.ClearAllEventHandlers();
            return (int64_t)0;
        });

        // ========== Input Functions ==========
        vm.RegisterNative("Input.IsKeyPressed", [](const std::vector<Value>& args) -> Value {
            if (args.empty() || !g_InputHooks.IsKeyPressed) {
                return (int64_t)0;
            }
            return static_cast<int64_t>(g_InputHooks.IsKeyPressed(static_cast<int>(GetInt(args[0]))) ? 1 : 0);
        });
        
        vm.RegisterNative("Input.IsKeyJustPressed", [](const std::vector<Value>& args) -> Value {
            if (args.empty() || !g_InputHooks.IsKeyJustPressed) {
                return (int64_t)0;
            }
            return static_cast<int64_t>(g_InputHooks.IsKeyJustPressed(static_cast<int>(GetInt(args[0]))) ? 1 : 0);
        });
        
        vm.RegisterNative("Input.IsKeyJustReleased", [](const std::vector<Value>& args) -> Value {
            if (args.empty() || !g_InputHooks.IsKeyJustReleased) {
                return (int64_t)0;
            }
            return static_cast<int64_t>(g_InputHooks.IsKeyJustReleased(static_cast<int>(GetInt(args[0]))) ? 1 : 0);
        });
        
        vm.RegisterNative("Input.IsMouseButtonPressed", [](const std::vector<Value>& args) -> Value {
            if (args.empty() || !g_InputHooks.IsMouseButtonPressed) {
                return (int64_t)0;
            }
            return static_cast<int64_t>(g_InputHooks.IsMouseButtonPressed(static_cast<int>(GetInt(args[0]))) ? 1 : 0);
        });
        
        vm.RegisterNative("Input.GetMousePosition", [](const std::vector<Value>& args) -> Value {
            (void)args;
            return g_InputHooks.GetMousePosition ? g_InputHooks.GetMousePosition() : Vec2();
        });
        
        vm.RegisterNative("Input.GetMouseDelta", [](const std::vector<Value>& args) -> Value {
            (void)args;
            return g_InputHooks.GetMouseDelta ? g_InputHooks.GetMouseDelta() : Vec2();
        });

        // ========== UI Functions ==========
        // Layout
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

        vm.RegisterNative("UI.SameLine", [](const std::vector<Value>& args) -> Value {
            UI::Widgets::SameLine();
            return (int64_t)0;
        });

        vm.RegisterNative("UI.Separator", [](const std::vector<Value>& args) -> Value {
            UI::Widgets::Separator();
            return (int64_t)0;
        });

        vm.RegisterNative("UI.Spacing", [](const std::vector<Value>& args) -> Value {
            UI::Widgets::Spacing();
            return (int64_t)0;
        });

        vm.RegisterNative("UI.CollapsingHeader", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0) {
                std::string label = GetString(args[0]);
                bool defaultOpen = args.size() > 1 ? (GetInt(args[1]) != 0) : false;
                return (int64_t)(UI::Widgets::CollapsingHeader(label, defaultOpen) ? 1 : 0);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("UI.BeginChild", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0) {
                std::string id = GetString(args[0]);
                float width = args.size() > 1 ? GetFloat(args[1]) : 0.0f;
                float height = args.size() > 2 ? GetFloat(args[2]) : 0.0f;
                bool border = args.size() > 3 ? (GetInt(args[3]) != 0) : false;
                UI::Widgets::BeginChild(id, width, height, border);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("UI.EndChild", [](const std::vector<Value>& args) -> Value {
            UI::Widgets::EndChild();
            return (int64_t)0;
        });

        vm.RegisterNative("UI.Tooltip", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0) {
                UI::Widgets::Tooltip(GetString(args[0]));
            }
            return (int64_t)0;
        });

        // Text
        vm.RegisterNative("UI.Text", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0) {
                UI::Widgets::Text(GetString(args[0]));
            }
            return (int64_t)0;
        });

        vm.RegisterNative("UI.Label", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2) {
                UI::Widgets::Label(GetString(args[0]), GetString(args[1]));
            }
            return (int64_t)0;
        });

        vm.RegisterNative("UI.TextBold", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0) {
                UI::Widgets::TextBold(GetString(args[0]));
            }
            return (int64_t)0;
        });

        vm.RegisterNative("UI.Button", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0) {
                bool clicked = UI::Widgets::Button(GetString(args[0]));
                return (int64_t)(clicked ? 1 : 0);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("UI.Checkbox", [](const std::vector<Value>& args) -> Value {
            // Note: This requires mutable value passing - simplified for now
            // In a real implementation, you'd use a register or reference system
            if (args.size() >= 2) {
                std::string label = GetString(args[0]);
                bool value = GetInt(args[1]) != 0;
                bool changed = UI::Widgets::Checkbox(label, value);
                // Return both changed flag and new value - scripts would need to handle this
                return (int64_t)(changed ? (value ? 2 : 1) : (value ? 1 : 0));
            }
            return (int64_t)0;
        });

        vm.RegisterNative("UI.SliderFloat", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 4) {
                std::string label = GetString(args[0]);
                float value = GetFloat(args[1]);
                float min = GetFloat(args[2]);
                float max = GetFloat(args[3]);
                bool changed = UI::Widgets::SliderFloat(label, value, min, max);
                // Return value as double - scripts need to extract it
                return (double)value;
            }
            return (double)0.0;
        });

        vm.RegisterNative("UI.SliderInt", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 4) {
                std::string label = GetString(args[0]);
                int value = (int)GetInt(args[1]);
                int min = (int)GetInt(args[2]);
                int max = (int)GetInt(args[3]);
                bool changed = UI::Widgets::SliderInt(label, value, min, max);
                return (int64_t)value;
            }
            return (int64_t)0;
        });

        vm.RegisterNative("UI.InputText", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2) {
                std::string label = GetString(args[0]);
                std::string value = GetString(args[1]);
                bool changed = UI::Widgets::InputText(label, value);
                return value; // Return modified string
            }
            return std::string("");
        });

        vm.RegisterNative("UI.ProgressBar", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0) {
                float fraction = GetFloat(args[0]);
                std::string overlay = args.size() > 1 ? GetString(args[1]) : "";
                UI::Widgets::ProgressBar(fraction, overlay);
            }
            return (int64_t)0;
        });

        // ========== Physics Functions ==========
        if (registry && physicsSystem) {
            vm.RegisterNative("Physics.AddRigidBody", [registry](const std::vector<Value>& args) -> Value {
                if (args.size() >= 2) {
                    ECS::EntityId id = (ECS::EntityId)GetInt(args[0]);
                    std::string type = GetString(args[1]);

                    Physics::RigidBody rb;

                    if (type == "Sphere") {
                        rb.Type = Physics::ColliderType::Sphere;
                        rb.Radius = args.size() > 2 ? GetFloat(args[2]) : 0.5f;
                    } else if (type == "Box") {
                        rb.Type = Physics::ColliderType::Box;
                        if (args.size() >= 5) {
                            rb.HalfExtents = Vec3(GetFloat(args[2]), GetFloat(args[3]), GetFloat(args[4]));
                        }
                    } else if (type == "Capsule") {
                        rb.Type = Physics::ColliderType::Capsule;
                        rb.CapsuleHeight = args.size() > 2 ? GetFloat(args[2]) : 1.0f;
                        rb.CapsuleRadius = args.size() > 3 ? GetFloat(args[3]) : 0.5f;
                    }

                    registry->Add<Physics::RigidBody>(id, rb);
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.CreateSoftBodyCloth", [registry, physicsSystem](const std::vector<Value>& args) -> Value {
                if (!registry || !physicsSystem || args.size() < 7) {
                    return (int64_t)0;
                }

                const ECS::EntityId id = (ECS::EntityId)GetInt(args[0]);
                const Math::Vec3 origin(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3]));

                Physics::SoftBodyConfig cfg{};
                cfg.GridWidth = static_cast<uint32_t>(std::max<int64_t>(2, GetInt(args[4])));
                cfg.GridHeight = static_cast<uint32_t>(std::max<int64_t>(2, GetInt(args[5])));
                cfg.NodeSpacing = std::max(0.01f, GetFloat(args[6]));
                cfg.NodeMass = args.size() > 7 ? std::max(0.001f, GetFloat(args[7])) : 1.0f;
                cfg.SolverIterations = args.size() > 8 ? std::max(1, (int)GetInt(args[8])) : 8;
                cfg.Damping = args.size() > 9 ? std::clamp(GetFloat(args[9]), 0.0f, 1.0f) : 0.05f;
                cfg.AnchorTopRow = args.size() > 10 ? (GetInt(args[10]) != 0) : true;

                physicsSystem->CreateSoftBodyStub(id, cfg);
                if (auto* sb = registry->TryGet<Physics::SoftBody>(id)) {
                    sb->BuildRectCloth(origin, cfg);
                }
                return (int64_t)1;
            });

            vm.RegisterNative("Physics.CreateVehicle", [registry, physicsSystem](const std::vector<Value>& args) -> Value {
                if (!registry || !physicsSystem || args.size() < 4) {
                    return (int64_t)0;
                }
                const ECS::EntityId id = (ECS::EntityId)GetInt(args[0]);
                Physics::VehicleConfig cfg{};
                cfg.WheelBase = std::max(0.5f, GetFloat(args[1]));
                cfg.TrackWidth = std::max(0.5f, GetFloat(args[2]));
                cfg.Mass = std::max(1.0f, GetFloat(args[3]));
                physicsSystem->CreateVehicleStub(id, cfg);
                return (int64_t)1;
            });

            vm.RegisterNative("Physics.SetVehicleInput", [registry](const std::vector<Value>& args) -> Value {
                if (!registry || args.size() < 4) {
                    return (int64_t)0;
                }
                const ECS::EntityId id = (ECS::EntityId)GetInt(args[0]);
                if (auto* v = registry->TryGet<Physics::Vehicle>(id)) {
                    v->ThrottleInput = std::clamp(GetFloat(args[1]), -1.0f, 1.0f);
                    v->SteeringInput = std::clamp(GetFloat(args[2]), -1.0f, 1.0f);
                    v->BrakeInput = std::clamp(GetFloat(args[3]), 0.0f, 1.0f);
                    if (args.size() > 4) {
                        v->Handbrake = GetInt(args[4]) != 0;
                    }
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.GetSoftBodyNodeCount", [registry](const std::vector<Value>& args) -> Value {
                if (!registry || args.empty()) {
                    return (int64_t)0;
                }
                const ECS::EntityId id = (ECS::EntityId)GetInt(args[0]);
                if (const auto* sb = registry->TryGet<Physics::SoftBody>(id)) {
                    return (int64_t)sb->Nodes.size();
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.GetSoftBodyNodePosition", [registry](const std::vector<Value>& args) -> Value {
                if (!registry || args.size() < 2) {
                    return Vec3();
                }
                const ECS::EntityId id = (ECS::EntityId)GetInt(args[0]);
                const size_t index = static_cast<size_t>(std::max<int64_t>(0, GetInt(args[1])));
                if (const auto* sb = registry->TryGet<Physics::SoftBody>(id)) {
                    if (index < sb->Nodes.size()) {
                        return sb->Nodes[index].Position;
                    }
                }
                return Vec3();
            });

            vm.RegisterNative("Physics.SetSoftBodyWind", [registry](const std::vector<Value>& args) -> Value {
                if (!registry || args.size() < 4) {
                    return (int64_t)0;
                }
                const ECS::EntityId id = (ECS::EntityId)GetInt(args[0]);
                if (auto* sb = registry->TryGet<Physics::SoftBody>(id)) {
                    sb->WindForce = Vec3(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3]));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.GetPosition", [registry](const std::vector<Value>& args) -> Value {
                if (args.size() > 0 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    const auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    return rb.Position;
                }
                return Vec3();
            });

            vm.RegisterNative("Physics.SetPosition", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (int64_t)0;
                if (args.size() >= 4 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    rb.Position = Vec3(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3]));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.GetVelocity", [registry](const std::vector<Value>& args) -> Value {
                if (args.size() > 0 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    const auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    return rb.Velocity;
                }
                return Vec3();
            });

            vm.RegisterNative("Physics.SetVelocity", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (int64_t)0;
                if (args.size() >= 4 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    rb.Velocity = Vec3(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3]));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.GetRotation", [registry](const std::vector<Value>& args) -> Value {
                if (args.size() > 0 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    const auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    return rb.Rotation;
                }
                return Quaternion();
            });

            vm.RegisterNative("Physics.SetRotation", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (int64_t)0;
                if (args.size() >= 5 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    rb.Rotation = Quaternion(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3]), GetFloat(args[4]));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.GetMass", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (double)0.0;
                if (args.size() > 0 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    const auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    return (double)rb.Mass;
                }
                return (double)0.0;
            });

            vm.RegisterNative("Physics.SetMass", [registry](const std::vector<Value>& args) -> Value {
                if (args.size() >= 2 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    rb.SetMass(GetFloat(args[1]));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.ApplyForce", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (int64_t)0;
                if (args.size() >= 4 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    rb.ApplyForce(Vec3(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3])));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.ApplyTorque", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (int64_t)0;
                if (args.size() >= 4 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    rb.ApplyTorque(Vec3(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3])));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.ApplyImpulse", [registry](const std::vector<Value>& args) -> Value {
                if (args.size() >= 4 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    rb.ApplyImpulse(Vec3(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3])));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.ApplyAngularImpulse", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (int64_t)0;
                if (args.size() >= 4 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    rb.ApplyAngularImpulse(Vec3(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3])));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.SetStatic", [registry](const std::vector<Value>& args) -> Value {
                if (args.size() >= 2 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    rb.IsStatic = GetInt(args[1]) != 0;
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.SetFriction", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (int64_t)0;
                if (args.size() >= 2 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    rb.Friction = GetFloat(args[1]);
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.SetRestitution", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (int64_t)0;
                if (args.size() >= 2 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    rb.Restitution = GetFloat(args[1]);
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.SetGravityScale", [registry](const std::vector<Value>& args) -> Value {
                if (args.size() >= 2 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    rb.GravityScale = GetFloat(args[1]);
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.SetVelocityIterations", [physicsSystem](const std::vector<Value>& args) -> Value {
                if (!physicsSystem) return (int64_t)0;
                if (args.size() > 0) {
                    physicsSystem->SetVelocityIterations((int)GetInt(args[0]));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Physics.SetPositionIterations", [physicsSystem](const std::vector<Value>& args) -> Value {
                if (!physicsSystem) return (int64_t)0;
                if (args.size() > 0) {
                    physicsSystem->SetPositionIterations((int)GetInt(args[0]));
                }
                return (int64_t)0;
            });
        }

        // ========== Array Operations ==========
        vm.RegisterNative("Array.New", [](const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                return std::make_shared<Array>();
            } else if (args.size() == 1 && std::holds_alternative<int64_t>(args[0])) {
                return std::make_shared<Array>((size_t)std::get<int64_t>(args[0]));
            } else {
                auto arr = std::make_shared<Array>();
                for (const auto& arg : args) {
                    arr->Push(arg);
                }
                return arr;
            }
        });

        vm.RegisterNative("Array.Get", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<std::shared_ptr<Array>>(args[0]) && std::holds_alternative<int64_t>(args[1])) {
                auto arr = std::get<std::shared_ptr<Array>>(args[0]);
                size_t index = (size_t)std::get<int64_t>(args[1]);
                return arr->Get(index);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Array.Set", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 3 && std::holds_alternative<std::shared_ptr<Array>>(args[0]) && std::holds_alternative<int64_t>(args[1])) {
                auto arr = std::get<std::shared_ptr<Array>>(args[0]);
                size_t index = (size_t)std::get<int64_t>(args[1]);
                arr->Set(index, args[2]);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Array.Push", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<std::shared_ptr<Array>>(args[0])) {
                auto arr = std::get<std::shared_ptr<Array>>(args[0]);
                arr->Push(args[1]);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Array.Pop", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<std::shared_ptr<Array>>(args[0])) {
                auto arr = std::get<std::shared_ptr<Array>>(args[0]);
                return arr->Pop();
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Array.Length", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<std::shared_ptr<Array>>(args[0])) {
                auto arr = std::get<std::shared_ptr<Array>>(args[0]);
                return (int64_t)arr->Length();
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Array.Clear", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<std::shared_ptr<Array>>(args[0])) {
                auto arr = std::get<std::shared_ptr<Array>>(args[0]);
                arr->Clear();
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Array.Insert", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 3 && std::holds_alternative<std::shared_ptr<Array>>(args[0]) && std::holds_alternative<int64_t>(args[1])) {
                auto arr = std::get<std::shared_ptr<Array>>(args[0]);
                size_t index = (size_t)std::get<int64_t>(args[1]);
                arr->Insert(index, args[2]);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Array.Remove", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<std::shared_ptr<Array>>(args[0]) && std::holds_alternative<int64_t>(args[1])) {
                auto arr = std::get<std::shared_ptr<Array>>(args[0]);
                size_t index = (size_t)std::get<int64_t>(args[1]);
                arr->Remove(index);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Array.Slice", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 3 && std::holds_alternative<std::shared_ptr<Array>>(args[0]) &&
                std::holds_alternative<int64_t>(args[1]) && std::holds_alternative<int64_t>(args[2])) {
                auto arr = std::get<std::shared_ptr<Array>>(args[0]);
                size_t start = (size_t)std::get<int64_t>(args[1]);
                size_t end = (size_t)std::get<int64_t>(args[2]);
                return std::make_shared<Array>(arr->Slice(start, end));
            }
            return std::make_shared<Array>();
        });

        vm.RegisterNative("Array.IndexOf", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<std::shared_ptr<Array>>(args[0])) {
                auto arr = std::get<std::shared_ptr<Array>>(args[0]);
                return arr->IndexOf(args[1]);
            }
            return (int64_t)-1;
        });

        // ========== Dictionary Operations ==========
        vm.RegisterNative("Dictionary.New", [](const std::vector<Value>& args) -> Value {
            return std::make_shared<Dictionary>();
        });

        vm.RegisterNative("Dictionary.Get", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<std::shared_ptr<Dictionary>>(args[0])) {
                auto dict = std::get<std::shared_ptr<Dictionary>>(args[0]);
                std::string key = GetString(args[1]);
                return dict->Get(key);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Dictionary.Set", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 3 && std::holds_alternative<std::shared_ptr<Dictionary>>(args[0])) {
                auto dict = std::get<std::shared_ptr<Dictionary>>(args[0]);
                std::string key = GetString(args[1]);
                dict->Set(key, args[2]);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Dictionary.Has", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<std::shared_ptr<Dictionary>>(args[0])) {
                auto dict = std::get<std::shared_ptr<Dictionary>>(args[0]);
                std::string key = GetString(args[1]);
                return (int64_t)(dict->Has(key) ? 1 : 0);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Dictionary.Remove", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<std::shared_ptr<Dictionary>>(args[0])) {
                auto dict = std::get<std::shared_ptr<Dictionary>>(args[0]);
                std::string key = GetString(args[1]);
                dict->Remove(key);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Dictionary.Keys", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<std::shared_ptr<Dictionary>>(args[0])) {
                auto dict = std::get<std::shared_ptr<Dictionary>>(args[0]);
                return dict->Keys();
            }
            return std::make_shared<Array>();
        });

        vm.RegisterNative("Dictionary.Values", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<std::shared_ptr<Dictionary>>(args[0])) {
                auto dict = std::get<std::shared_ptr<Dictionary>>(args[0]);
                return dict->Values();
            }
            return std::make_shared<Array>();
        });

        vm.RegisterNative("Dictionary.Size", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<std::shared_ptr<Dictionary>>(args[0])) {
                auto dict = std::get<std::shared_ptr<Dictionary>>(args[0]);
                return (int64_t)dict->Size();
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Dictionary.Clear", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<std::shared_ptr<Dictionary>>(args[0])) {
                auto dict = std::get<std::shared_ptr<Dictionary>>(args[0]);
                dict->Clear();
            }
            return (int64_t)0;
        });

        // ========== Set Operations ==========
        vm.RegisterNative("Set.New", [](const std::vector<Value>& args) -> Value {
            auto s = std::make_shared<Set>();
            for (const auto& arg : args) {
                s->Add(arg);
            }
            return s;
        });

        vm.RegisterNative("Set.Add", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<std::shared_ptr<Set>>(args[0])) {
                auto s = std::get<std::shared_ptr<Set>>(args[0]);
                s->Add(args[1]);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Set.Remove", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<std::shared_ptr<Set>>(args[0])) {
                auto s = std::get<std::shared_ptr<Set>>(args[0]);
                s->Remove(args[1]);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Set.Contains", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<std::shared_ptr<Set>>(args[0])) {
                auto s = std::get<std::shared_ptr<Set>>(args[0]);
                return (int64_t)(s->Contains(args[1]) ? 1 : 0);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Set.Size", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<std::shared_ptr<Set>>(args[0])) {
                auto s = std::get<std::shared_ptr<Set>>(args[0]);
                return (int64_t)s->Size();
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Set.Clear", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && std::holds_alternative<std::shared_ptr<Set>>(args[0])) {
                auto s = std::get<std::shared_ptr<Set>>(args[0]);
                s->Clear();
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Set.Union", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<std::shared_ptr<Set>>(args[0]) && std::holds_alternative<std::shared_ptr<Set>>(args[1])) {
                auto s1 = std::get<std::shared_ptr<Set>>(args[0]);
                auto s2 = std::get<std::shared_ptr<Set>>(args[1]);
                return s1->Union(*s2);
            }
            return std::make_shared<Set>();
        });

        vm.RegisterNative("Set.Intersection", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2 && std::holds_alternative<std::shared_ptr<Set>>(args[0]) && std::holds_alternative<std::shared_ptr<Set>>(args[1])) {
                auto s1 = std::get<std::shared_ptr<Set>>(args[0]);
                auto s2 = std::get<std::shared_ptr<Set>>(args[1]);
                return s1->Intersection(*s2);
            }
            return std::make_shared<Set>();
        });

        // ========== Renderer Functions ==========
        if (scene) {
            vm.RegisterNative("Render.AddObject", [scene](const std::vector<Value>& args) -> Value {
                if (args.size() >= 4) {
                    uint32_t meshId = (uint32_t)GetInt(args[0]);
                    float x = GetFloat(args[1]);
                    float y = GetFloat(args[2]);
                    float z = GetFloat(args[3]);
                    float rotW = args.size() > 4 ? GetFloat(args[4]) : 1.0f;
                    float rotX = args.size() > 5 ? GetFloat(args[5]) : 0.0f;
                    float rotY = args.size() > 6 ? GetFloat(args[6]) : 0.0f;
                    float rotZ = args.size() > 7 ? GetFloat(args[7]) : 0.0f;
                    float scaleX = args.size() > 8 ? GetFloat(args[8]) : 1.0f;
                    float scaleY = args.size() > 9 ? GetFloat(args[9]) : 1.0f;
                    float scaleZ = args.size() > 10 ? GetFloat(args[10]) : 1.0f;
                    uint8_t objectType = args.size() > 11 ? (uint8_t)GetInt(args[11]) : 0;
                    Render::SceneObjectID id = scene->AddObject(meshId, Vec3(x, y, z), Quaternion(rotW, rotX, rotY, rotZ), Vec3(scaleX, scaleY, scaleZ), (Render::ObjectType)objectType);
                    return (int64_t)id;
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Render.RemoveObject", [scene](const std::vector<Value>& args) -> Value {
                if (!scene) return (int64_t)0;
                if (args.size() > 0) {
                    scene->RemoveObject((Render::SceneObjectID)GetInt(args[0]));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Render.SetTransform", [scene](const std::vector<Value>& args) -> Value {
                if (args.size() >= 4 && std::holds_alternative<int64_t>(args[0])) {
                    int64_t id = std::get<int64_t>(args[0]);
                    double x = GetFloat(args[1]);
                    double y = GetFloat(args[2]);
                    double z = GetFloat(args[3]);
                    float rotW = args.size() > 4 ? GetFloat(args[4]) : 1.0f;
                    float rotX = args.size() > 5 ? GetFloat(args[5]) : 0.0f;
                    float rotY = args.size() > 6 ? GetFloat(args[6]) : 0.0f;
                    float rotZ = args.size() > 7 ? GetFloat(args[7]) : 0.0f;
                    float scaleX = args.size() > 8 ? GetFloat(args[8]) : 1.0f;
                    float scaleY = args.size() > 9 ? GetFloat(args[9]) : 1.0f;
                    float scaleZ = args.size() > 10 ? GetFloat(args[10]) : 1.0f;
                    scene->SetTransform((Render::SceneObjectID)id, Vec3((float)x, (float)y, (float)z), Quaternion(rotW, rotX, rotY, rotZ), Vec3(scaleX, scaleY, scaleZ));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Render.SetPosition", [scene](const std::vector<Value>& args) -> Value {
                if (!scene) return (int64_t)0;
                if (args.size() >= 4) {
                    Render::SceneObjectID id = (Render::SceneObjectID)GetInt(args[0]);
                    float x = GetFloat(args[1]);
                    float y = GetFloat(args[2]);
                    float z = GetFloat(args[3]);
                    scene->SetPosition(id, Vec3(x, y, z));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Render.SetRotation", [scene](const std::vector<Value>& args) -> Value {
                if (args.size() >= 5) {
                    Render::SceneObjectID id = (Render::SceneObjectID)GetInt(args[0]);
                    float w = GetFloat(args[1]);
                    float x = GetFloat(args[2]);
                    float y = GetFloat(args[3]);
                    float z = GetFloat(args[4]);
                    scene->SetRotation(id, Quaternion(w, x, y, z));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Render.SetScale", [scene](const std::vector<Value>& args) -> Value {
                if (!scene) return (int64_t)0;
                if (args.size() >= 4) {
                    Render::SceneObjectID id = (Render::SceneObjectID)GetInt(args[0]);
                    float x = GetFloat(args[1]);
                    float y = GetFloat(args[2]);
                    float z = GetFloat(args[3]);
                    // Use SetTransform with current position and rotation
                    Vec3 pos = scene->GetPosition(id);
                    Quaternion rot = scene->GetRotation(id);
                    scene->SetTransform(id, pos, rot, Vec3(x, y, z));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Render.GetPosition", [scene](const std::vector<Value>& args) -> Value {
                if (args.size() > 0) {
                    Render::SceneObjectID id = (Render::SceneObjectID)GetInt(args[0]);
                    return scene->GetPosition(id);
                }
                return Vec3();
            });

            vm.RegisterNative("Render.GetRotation", [scene](const std::vector<Value>& args) -> Value {
                if (!scene) return Quaternion();
                if (args.size() > 0) {
                    Render::SceneObjectID id = (Render::SceneObjectID)GetInt(args[0]);
                    return scene->GetRotation(id);
                }
                return Quaternion();
            });

            vm.RegisterNative("Render.GetScale", [scene](const std::vector<Value>& args) -> Value {
                if (!scene) return Vec3();
                if (args.size() > 0) {
                    Render::SceneObjectID id = (Render::SceneObjectID)GetInt(args[0]);
                    return scene->GetScale(id);
                }
                return Vec3();
            });

            vm.RegisterNative("Render.GetObjectCount", [scene](const std::vector<Value>& args) -> Value {
                return (int64_t)scene->GetObjectCount();
            });

            vm.RegisterNative("Render.UpdateTransforms", [scene](const std::vector<Value>& args) -> Value {
                if (!scene) return (int64_t)0;
                scene->UpdateTransforms();
                return (int64_t)0;
            });
        }

        // ========== Camera Operations ==========
        if (camera) {
            vm.RegisterNative("Render.Camera.GetPosition", [camera](const std::vector<Value>& args) -> Value {
                return camera->GetPosition();
            });

            vm.RegisterNative("Render.Camera.SetPosition", [camera](const std::vector<Value>& args) -> Value {
                if (!camera) return (int64_t)0;
                if (args.size() >= 3) {
                    camera->Position = Vec3(GetFloat(args[0]), GetFloat(args[1]), GetFloat(args[2]));
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Render.Camera.GetFront", [camera](const std::vector<Value>& args) -> Value {
                return camera->GetFront();
            });

            vm.RegisterNative("Render.Camera.GetZoom", [camera](const std::vector<Value>& args) -> Value {
                if (!camera) return (double)0.0;
                return (double)camera->GetZoom();
            });

            vm.RegisterNative("Render.Camera.ProcessKeyboard", [camera](const std::vector<Value>& args) -> Value {
                if (args.size() >= 4) {
                    Vec3 direction(GetFloat(args[0]), GetFloat(args[1]), GetFloat(args[2]));
                    float deltaTime = GetFloat(args[3]);
                    camera->ProcessKeyboard(direction, deltaTime);
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Render.Camera.ProcessMouseMovement", [camera](const std::vector<Value>& args) -> Value {
                if (!camera) return (int64_t)0;
                if (args.size() >= 2) {
                    float xOffset = GetFloat(args[0]);
                    float yOffset = GetFloat(args[1]);
                    bool constrainPitch = args.size() > 2 ? (GetInt(args[2]) != 0) : true;
                    camera->ProcessMouseMovement(xOffset, yOffset, constrainPitch);
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Render.Camera.ProcessMouseScroll", [camera](const std::vector<Value>& args) -> Value {
                if (args.size() > 0) {
                    float yOffset = GetFloat(args[0]);
                    camera->ProcessMouseScroll(yOffset);
                }
                return (int64_t)0;
            });

            vm.RegisterNative("Render.Camera.GetViewMatrix", [camera](const std::vector<Value>& args) -> Value {
                if (!camera) return Matrix4();
                return camera->GetViewMatrix();
            });
        }

        // ========== Arzachel (Procedural & Animation) ==========
        vm.RegisterNative("Arzachel.PlayAnimation", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2) {
                // ECS::EntityId entity = (ECS::EntityId)GetInt(args[0]);
                // std::string animName = GetString(args[1]);
                // Logic to trigger animation playback
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Arzachel.Generate", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 1) {
                // std::string type = GetString(args[0]);
                // Arzachel::Generator::Generate(type, ...);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Arzachel.Facial.PatternWeight", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 3) return (double)1.0;
            const int pattern = (int)GetInt(args[0]);
            const float period = GetFloat(args[1]);
            const float time = GetFloat(args[2]);
            const uint64_t seed = args.size() > 3 ? (uint64_t)GetInt(args[3]) : 1ull;
            const auto p = static_cast<Arzachel::ExpressionPattern>(
                std::clamp(pattern, (int)Arzachel::ExpressionPattern::Static, (int)Arzachel::ExpressionPattern::Sequence));
            return (double)Arzachel::PatternWeight(p, period, time, Arzachel::Seed(seed));
        });

        vm.RegisterNative("Arzachel.Facial.Blink", [](const std::vector<Value>& args) -> Value {
            if (args.empty()) return (double)0.0;
            const float time = GetFloat(args[0]);
            const uint64_t seed = args.size() > 1 ? (uint64_t)GetInt(args[1]) : 1ull;
            return (double)Arzachel::BlinkClosedAmount(time, Arzachel::Seed(seed));
        });

        vm.RegisterNative("Arzachel.Facial.SaccadeX", [](const std::vector<Value>& args) -> Value {
            if (args.empty()) return (double)0.0;
            const float time = GetFloat(args[0]);
            const uint64_t seed = args.size() > 1 ? (uint64_t)GetInt(args[1]) : 1ull;
            return (double)Arzachel::SaccadeOffset(time, Arzachel::Seed(seed)).x;
        });

        vm.RegisterNative("Arzachel.Facial.SaccadeY", [](const std::vector<Value>& args) -> Value {
            if (args.empty()) return (double)0.0;
            const float time = GetFloat(args[0]);
            const uint64_t seed = args.size() > 1 ? (uint64_t)GetInt(args[1]) : 1ull;
            return (double)Arzachel::SaccadeOffset(time, Arzachel::Seed(seed)).y;
        });

        vm.RegisterNative("Arzachel.LipSync.CountVisemes", [](const std::vector<Value>& args) -> Value {
            if (args.empty()) return (int64_t)0;
            const auto samples = Arzachel::TextToVisemeStrengthSamples(GetString(args[0]));
            return (int64_t)samples.size();
        });

        vm.RegisterNative("Arzachel.LipSync.MakeKeyframes", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 5) return (int64_t)0;
            std::vector<Arzachel::VisemeKeyframeTick> keys;
            std::string warn;
            Arzachel::LipSyncTextConfig cfg{};
            if (args.size() > 5) cfg.Coarticulation = std::clamp(GetFloat(args[5]), 0.0f, 1.0f);
            Arzachel::BuildVisemeKeyframesFromEnglishTextEx(
                GetString(args[0]),
                (uint32_t)GetInt(args[1]),
                (uint64_t)GetInt(args[2]),
                (uint64_t)GetInt(args[3]),
                (uint32_t)GetInt(args[4]),
                cfg,
                keys,
                warn
            );
            return (int64_t)keys.size();
        });

        vm.RegisterNative("Arzachel.Facial.EnvelopeWeight", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 4) return (double)0.0;
            Arzachel::ExpressionEnvelope env{};
            env.AttackSec = std::max(0.001f, GetFloat(args[1]));
            env.HoldSec = std::max(0.0f, GetFloat(args[2]));
            env.ReleaseSec = std::max(0.001f, GetFloat(args[3]));
            env.Shape = args.size() > 4 ? std::max(0.2f, GetFloat(args[4])) : 1.0f;
            return (double)Arzachel::EvaluateEnvelopeWeight(GetFloat(args[0]), env);
        });

        vm.RegisterNative("Arzachel.LipSync.SampleBlend", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 6) return (double)0.0;
            std::vector<Arzachel::VisemeKeyframeTick> keys;
            keys.push_back(Arzachel::VisemeKeyframeTick{(uint64_t)GetInt(args[0]), GetString(args[1]), GetFloat(args[2])});
            keys.push_back(Arzachel::VisemeKeyframeTick{(uint64_t)GetInt(args[3]), GetString(args[4]), GetFloat(args[5])});
            std::string cur, next;
            float blend = 0.0f;
            float strength = 0.0f;
            const uint64_t tick = args.size() > 6 ? (uint64_t)GetInt(args[6]) : (uint64_t)GetInt(args[0]);
            Arzachel::SampleVisemeAtTick(keys, tick, cur, next, blend, strength);
            return (double)blend;
        });

        vm.RegisterNative("Arzachel.FacialState.Set", [registry](const std::vector<Value>& args) -> Value {
            if (!registry || args.size() < 2) return (int64_t)0;
            const ECS::EntityId e = static_cast<ECS::EntityId>(GetInt(args[0]));
            if (!registry->Valid(e)) return (int64_t)0;
            auto* f = registry->TryGet<ECS::FacialStateMachine>(e);
            if (!f) return (int64_t)0;
            const int state = std::clamp((int)GetInt(args[1]), 0, 3);
            f->State = static_cast<ECS::FacialIntentState>(state);
            return (int64_t)1;
        });

        vm.RegisterNative("Arzachel.FacialState.SetMood", [registry](const std::vector<Value>& args) -> Value {
            if (!registry || args.size() < 2) return (int64_t)0;
            const ECS::EntityId e = static_cast<ECS::EntityId>(GetInt(args[0]));
            if (!registry->Valid(e)) return (int64_t)0;
            auto* f = registry->TryGet<ECS::FacialStateMachine>(e);
            if (!f) return (int64_t)0;
            f->MoodName = GetString(args[1]);
            return (int64_t)1;
        });

        vm.RegisterNative("Arzachel.FacialState.SetEmote", [registry](const std::vector<Value>& args) -> Value {
            if (!registry || args.size() < 2) return (int64_t)0;
            const ECS::EntityId e = static_cast<ECS::EntityId>(GetInt(args[0]));
            if (!registry->Valid(e)) return (int64_t)0;
            auto* f = registry->TryGet<ECS::FacialStateMachine>(e);
            if (!f) return (int64_t)0;
            f->EmoteName = GetString(args[1]);
            return (int64_t)1;
        });

        vm.RegisterNative("Arzachel.FacialState.SetDialogue", [registry](const std::vector<Value>& args) -> Value {
            if (!registry || args.size() < 2) return (int64_t)0;
            const ECS::EntityId e = static_cast<ECS::EntityId>(GetInt(args[0]));
            if (!registry->Valid(e)) return (int64_t)0;
            auto* f = registry->TryGet<ECS::FacialStateMachine>(e);
            if (!f) return (int64_t)0;
            f->DialogueText = GetString(args[1]);
            f->DialogueDirty = true;
            if (args.size() > 2) f->DialogueStartTick = static_cast<uint64_t>(GetInt(args[2]));
            if (args.size() > 3) f->DialogueEndTick = static_cast<uint64_t>(GetInt(args[3]));
            return (int64_t)1;
        });

        // ========== Audio Functions ==========
        vm.RegisterNative("Audio.PlayMusic", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 1) {
                try {
                    std::string path = GetString(args[0]);
                    int loop = args.size() > 1 ? (int)GetInt(args[1]) : -1;
                    Core::Audio::AudioManager::Instance().PlayMusic(path.c_str(), loop);
                } catch (const std::exception& e) {
                    std::cerr << "ERROR: Exception in Audio.PlayMusic: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "ERROR: Unknown exception in Audio.PlayMusic" << std::endl;
                }
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Audio.PlaySound3D", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 4) {
                try {
                    std::string path = GetString(args[0]);
                    Vec3 pos(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3]));
                    float radius = args.size() > 4 ? GetFloat(args[4]) : 10.0f;
                    bool loop = args.size() > 5 ? (GetInt(args[5]) != 0) : false;
                    Core::Audio::AudioSource source = Core::Audio::AudioManager::Instance().PlaySound3D(path.c_str(), pos, radius, loop);
                    // Note: We don't store the source, but that's okay for script usage
                    // The audio will play and stop automatically when done (unless looping)
                } catch (const std::exception& e) {
                    std::cerr << "ERROR: Exception in Audio.PlaySound3D: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "ERROR: Unknown exception in Audio.PlaySound3D" << std::endl;
                }
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Audio.CreateEmitter", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 4) {
                return (int64_t)0;
            }
            try {
                std::string path = GetString(args[0]);
                Vec3 pos(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3]));
                float maxDistance = args.size() > 4 ? GetFloat(args[4]) : 50.0f;
                bool loop = args.size() > 5 ? (GetInt(args[5]) != 0) : false;
                auto handle = Core::Audio::AudioManager::Instance().CreateEmitter(
                    path.c_str(),
                    Math::Vec3(pos.x, pos.y, pos.z),
                    maxDistance,
                    loop
                );
                return static_cast<int64_t>(handle);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.UpdateEmitterTransform", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 4) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                Math::Vec3 pos(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3]));
                bool ok = Core::Audio::AudioManager::Instance().UpdateEmitterTransform(handle, pos);
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.SetEmitterVolume", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 2) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                bool ok = Core::Audio::AudioManager::Instance().SetEmitterVolume(handle, GetFloat(args[1]));
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.SetEmitterOcclusion", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 2) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                bool ok = Core::Audio::AudioManager::Instance().SetEmitterOcclusion(handle, GetFloat(args[1]));
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.SetEmitterRolloff", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 5) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                float minDistance = GetFloat(args[1]);
                float maxDistance = GetFloat(args[2]);
                float rolloff = GetFloat(args[3]);
                int modelInt = static_cast<int>(GetInt(args[4]));
                Core::Audio::DistanceModel model = Core::Audio::DistanceModel::Inverse;
                if (modelInt >= static_cast<int>(Core::Audio::DistanceModel::Linear)
                    && modelInt <= static_cast<int>(Core::Audio::DistanceModel::Exponential)) {
                    model = static_cast<Core::Audio::DistanceModel>(modelInt);
                }
                bool ok = Core::Audio::AudioManager::Instance().SetEmitterRolloff(handle, minDistance, maxDistance, rolloff, model);
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.SetEmitterDirection", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 4) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                Math::Vec3 dir(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3]));
                bool ok = Core::Audio::AudioManager::Instance().SetEmitterDirection(handle, dir);
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.SetEmitterCone", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 4) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                bool ok = Core::Audio::AudioManager::Instance().SetEmitterCone(
                    handle,
                    GetFloat(args[1]),
                    GetFloat(args[2]),
                    GetFloat(args[3])
                );
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.SetEmitterDoppler", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 3) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                bool ok = Core::Audio::AudioManager::Instance().SetEmitterDoppler(
                    handle,
                    GetFloat(args[1]),
                    GetFloat(args[2])
                );
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.SetEmitterVelocity", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 4) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                Math::Vec3 velocity(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3]));
                bool manual = args.size() > 4 ? (GetInt(args[4]) != 0) : true;
                bool ok = Core::Audio::AudioManager::Instance().SetEmitterVelocity(handle, velocity, manual);
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.ClearEmitterVelocity", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 1) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                bool ok = Core::Audio::AudioManager::Instance().ClearEmitterVelocity(handle);
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.SetEmitterFocus", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 4) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                bool ok = Core::Audio::AudioManager::Instance().SetEmitterFocus(
                    handle,
                    GetFloat(args[1]),
                    GetFloat(args[2]),
                    GetFloat(args[3])
                );
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.SetEmitterImmersion", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 4) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                bool ok = Core::Audio::AudioManager::Instance().SetEmitterImmersion(
                    handle,
                    GetFloat(args[1]),
                    GetFloat(args[2]),
                    GetFloat(args[3])
                );
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.SetEmitterDiffraction", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 3) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                bool ok = Core::Audio::AudioManager::Instance().SetEmitterDiffraction(
                    handle,
                    GetFloat(args[1]),
                    GetFloat(args[2])
                );
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.SetEmitterMotionAdaptation", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 2) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                bool ok = Core::Audio::AudioManager::Instance().SetEmitterMotionAdaptation(handle, GetFloat(args[1]));
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.SetEmitterAirAbsorption", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 2) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                bool ok = Core::Audio::AudioManager::Instance().SetEmitterAirAbsorption(handle, GetFloat(args[1]));
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.SetEmitterPitchVariance", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 2) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                bool ok = Core::Audio::AudioManager::Instance().SetEmitterPitchVariance(handle, GetFloat(args[1]));
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.SetEmitterFlags", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 4) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                const bool isDialogue = GetInt(args[1]) != 0;
                const bool isCriticalCue = GetInt(args[2]) != 0;
                const int priority = static_cast<int>(GetInt(args[3]));
                bool ok = Core::Audio::AudioManager::Instance().SetEmitterFlags(handle, isDialogue, isCriticalCue, priority);
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.ApplySpatialProfile", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 2) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                const int profile = static_cast<int>(GetInt(args[1]));
                auto& audio = Core::Audio::AudioManager::Instance();
                bool ok = true;
                switch (profile) {
                    case 1: // AmbientBed
                        ok = audio.SetEmitterFocus(handle, 0.05f, 0.03f, 0.95f) && ok;
                        ok = audio.SetEmitterImmersion(handle, 0.75f, 0.65f, 0.45f) && ok;
                        ok = audio.SetEmitterDiffraction(handle, 0.65f, 0.20f) && ok;
                        ok = audio.SetEmitterMotionAdaptation(handle, 0.90f) && ok;
                        ok = audio.SetEmitterRolloff(handle, 2.0f, 160.0f, 0.4f, Core::Audio::DistanceModel::Linear) && ok;
                        ok = audio.SetEmitterAirAbsorption(handle, 0.12f) && ok;
                        ok = audio.SetEmitterDoppler(handle, 0.15f, 343.3f) && ok;
                        ok = audio.SetEmitterCone(handle, 360.0f, 360.0f, 1.0f) && ok;
                        break;
                    case 2: // Footstep
                        ok = audio.SetEmitterFocus(handle, 0.30f, 0.07f, 1.0f) && ok;
                        ok = audio.SetEmitterImmersion(handle, 0.45f, 0.30f, 0.18f) && ok;
                        ok = audio.SetEmitterDiffraction(handle, 0.35f, 0.10f) && ok;
                        ok = audio.SetEmitterMotionAdaptation(handle, 0.35f) && ok;
                        ok = audio.SetEmitterRolloff(handle, 0.7f, 24.0f, 1.2f, Core::Audio::DistanceModel::Inverse) && ok;
                        ok = audio.SetEmitterPitchVariance(handle, 0.05f) && ok;
                        ok = audio.SetEmitterAirAbsorption(handle, 0.45f) && ok;
                        ok = audio.SetEmitterDoppler(handle, 0.7f, 343.3f) && ok;
                        ok = audio.SetEmitterCone(handle, 300.0f, 360.0f, 0.9f) && ok;
                        break;
                    case 3: // Weapon
                        ok = audio.SetEmitterFocus(handle, 0.55f, 0.10f, 1.05f) && ok;
                        ok = audio.SetEmitterImmersion(handle, 0.60f, 0.50f, 0.28f) && ok;
                        ok = audio.SetEmitterDiffraction(handle, 0.55f, 0.16f) && ok;
                        ok = audio.SetEmitterMotionAdaptation(handle, 0.40f) && ok;
                        ok = audio.SetEmitterRolloff(handle, 1.2f, 180.0f, 1.0f, Core::Audio::DistanceModel::Inverse) && ok;
                        ok = audio.SetEmitterPitchVariance(handle, 0.02f) && ok;
                        ok = audio.SetEmitterAirAbsorption(handle, 0.28f) && ok;
                        ok = audio.SetEmitterDoppler(handle, 1.0f, 343.3f) && ok;
                        ok = audio.SetEmitterCone(handle, 85.0f, 180.0f, 0.45f) && ok;
                        break;
                    case 4: // Voice
                        ok = audio.SetEmitterFocus(handle, 0.85f, 0.12f, 1.1f) && ok;
                        ok = audio.SetEmitterImmersion(handle, 0.70f, 0.42f, 0.35f) && ok;
                        ok = audio.SetEmitterDiffraction(handle, 0.50f, 0.22f) && ok;
                        ok = audio.SetEmitterMotionAdaptation(handle, 0.30f) && ok;
                        ok = audio.SetEmitterRolloff(handle, 0.8f, 42.0f, 1.35f, Core::Audio::DistanceModel::Inverse) && ok;
                        ok = audio.SetEmitterPitchVariance(handle, 0.01f) && ok;
                        ok = audio.SetEmitterAirAbsorption(handle, 0.33f) && ok;
                        ok = audio.SetEmitterDoppler(handle, 0.85f, 343.3f) && ok;
                        ok = audio.SetEmitterCone(handle, 60.0f, 140.0f, 0.35f) && ok;
                        ok = audio.SetEmitterFlags(handle, true, false, 2) && ok;
                        break;
                    case 5: // Vehicle
                        ok = audio.SetEmitterFocus(handle, 0.35f, 0.08f, 1.05f) && ok;
                        ok = audio.SetEmitterImmersion(handle, 0.85f, 0.58f, 0.42f) && ok;
                        ok = audio.SetEmitterDiffraction(handle, 0.70f, 0.18f) && ok;
                        ok = audio.SetEmitterMotionAdaptation(handle, 0.75f) && ok;
                        ok = audio.SetEmitterRolloff(handle, 2.5f, 260.0f, 0.9f, Core::Audio::DistanceModel::Exponential) && ok;
                        ok = audio.SetEmitterPitchVariance(handle, 0.015f) && ok;
                        ok = audio.SetEmitterAirAbsorption(handle, 0.22f) && ok;
                        ok = audio.SetEmitterDoppler(handle, 1.2f, 343.3f) && ok;
                        ok = audio.SetEmitterCone(handle, 100.0f, 220.0f, 0.5f) && ok;
                        break;
                    case 0: // Default
                    default:
                        ok = audio.SetEmitterFocus(handle, 0.15f, 0.05f, 1.0f) && ok;
                        ok = audio.SetEmitterImmersion(handle, 0.35f, 0.40f, 0.25f) && ok;
                        ok = audio.SetEmitterDiffraction(handle, 0.35f, 0.12f) && ok;
                        ok = audio.SetEmitterMotionAdaptation(handle, 0.20f) && ok;
                        ok = audio.SetEmitterRolloff(handle, 1.0f, 50.0f, 1.0f, Core::Audio::DistanceModel::Inverse) && ok;
                        ok = audio.SetEmitterPitchVariance(handle, 0.03f) && ok;
                        ok = audio.SetEmitterAirAbsorption(handle, 0.25f) && ok;
                        ok = audio.SetEmitterDoppler(handle, 1.0f, 343.3f) && ok;
                        ok = audio.SetEmitterCone(handle, 360.0f, 360.0f, 1.0f) && ok;
                        break;
                }
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.DestroyEmitter", [](const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                return (int64_t)0;
            }
            try {
                auto handle = static_cast<Core::Audio::AudioEmitterHandle>(GetInt(args[0]));
                bool ok = Core::Audio::AudioManager::Instance().DestroyEmitter(handle);
                return static_cast<int64_t>(ok ? 1 : 0);
            } catch (...) {
                return (int64_t)0;
            }
        });

        vm.RegisterNative("Audio.SetListener", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 9) {
                return (int64_t)0;
            }
            Core::Audio::Listener listener;
            listener.Position = Math::Vec3(GetFloat(args[0]), GetFloat(args[1]), GetFloat(args[2]));
            listener.Forward = Math::Vec3(GetFloat(args[3]), GetFloat(args[4]), GetFloat(args[5]));
            listener.Up = Math::Vec3(GetFloat(args[6]), GetFloat(args[7]), GetFloat(args[8]));
            listener.CurrentReverb = {0.0f, 0.0f, 1.0f};
            listener.TargetReverb = Core::Audio::ReverbPresetType::Room;
            Core::Audio::AudioManager::Instance().SetListener(listener);
            return (int64_t)1;
        });

        vm.RegisterNative("Audio.SetReverbPreset", [](const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                return (int64_t)0;
            }
            int preset = static_cast<int>(GetInt(args[0]));
            if (preset < static_cast<int>(Core::Audio::ReverbPresetType::None)
                || preset >= static_cast<int>(Core::Audio::ReverbPresetType::COUNT)) {
                preset = static_cast<int>(Core::Audio::ReverbPresetType::Room);
            }
            Core::Audio::AudioManager::Instance().SetReverbPreset(static_cast<Core::Audio::ReverbPresetType>(preset));
            return (int64_t)1;
        });

        vm.RegisterNative("Audio.SetVolume", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 1) {
                float vol = GetFloat(args[0]);
                Core::Audio::AudioManager::Instance().SetMasterVolume(vol);
            }
            return (int64_t)0;
        });

        vm.RegisterNative("Audio.HRTF.Enable", [](const std::vector<Value>& args) -> Value {
            const bool enabled = args.size() > 0 ? (GetInt(args[0]) != 0) : true;
            Core::Audio::AudioManager::Instance().SetHRTFEnabled(enabled);
            return (int64_t)1;
        });
        vm.RegisterNative("Audio.HRTF.LoadDatabase", [](const std::vector<Value>& args) -> Value {
            if (args.empty()) return (int64_t)0;
            const bool ok = Core::Audio::AudioManager::Instance().LoadHRTFDatabase(GetString(args[0]).c_str());
            return (int64_t)(ok ? 1 : 0);
        });
        vm.RegisterNative("Audio.HRTF.SetHeadRadius", [](const std::vector<Value>& args) -> Value {
            if (args.empty()) return (int64_t)0;
            Core::Audio::AudioManager::Instance().SetHeadRadius(GetFloat(args[0]));
            return (int64_t)1;
        });

        vm.RegisterNative("Audio.WaveTracing.Enable", [](const std::vector<Value>& args) -> Value {
            const bool enabled = args.size() > 0 ? (GetInt(args[0]) != 0) : true;
            Core::Audio::AudioManager::Instance().SetWaveTracingEnabled(enabled);
            return (int64_t)1;
        });
        vm.RegisterNative("Audio.WaveTracing.SetMaxRays", [](const std::vector<Value>& args) -> Value {
            if (args.empty()) return (int64_t)0;
            Core::Audio::AudioManager::Instance().SetWaveTracingMaxRays((int)GetInt(args[0]));
            return (int64_t)1;
        });
        vm.RegisterNative("Audio.WaveTracing.SetMaxBounces", [](const std::vector<Value>& args) -> Value {
            if (args.empty()) return (int64_t)0;
            Core::Audio::AudioManager::Instance().SetWaveTracingMaxBounces((int)GetInt(args[0]));
            return (int64_t)1;
        });

        vm.RegisterNative("Audio.Ambisonics.SetOrder", [](const std::vector<Value>& args) -> Value {
            if (args.empty()) return (int64_t)0;
            Core::Audio::AudioManager::Instance().SetAmbisonicOrder((int)GetInt(args[0]));
            return (int64_t)1;
        });
        vm.RegisterNative("Audio.Ambisonics.GetOrder", [](const std::vector<Value>&) -> Value {
            return (int64_t)Core::Audio::AudioManager::Instance().GetAmbisonicOrder();
        });

        vm.RegisterNative("Audio.FluidCoupling.Enable", [](const std::vector<Value>& args) -> Value {
            const bool enabled = args.size() > 0 ? (GetInt(args[0]) != 0) : true;
            Core::Audio::AudioManager::Instance().SetFluidCouplingEnabled(enabled);
            return (int64_t)1;
        });

        vm.RegisterNative("Audio.Portal.SetTransform", [](const std::vector<Value>& args) -> Value {
            if (args.size() < 16) return (int64_t)0;
            Math::Matrix4 m = Math::Matrix4::Identity();
            int k = 0;
            for (int row = 0; row < 4; ++row) {
                for (int col = 0; col < 4; ++col) {
                    m.M[row][col] = GetFloat(args[static_cast<size_t>(k++)]);
                }
            }
            const bool enabled = args.size() > 16 ? (GetInt(args[16]) != 0) : true;
            Core::Audio::AudioManager::Instance().SetPortalTransform(m, enabled);
            return (int64_t)1;
        });
        vm.RegisterNative("Audio.Portal.ClearTransform", [](const std::vector<Value>&) -> Value {
            Core::Audio::AudioManager::Instance().ClearPortalTransform();
            return (int64_t)1;
        });

        vm.RegisterNative("Audio.MLSpatialization.Enable", [](const std::vector<Value>& args) -> Value {
            const bool enabled = args.size() > 0 ? (GetInt(args[0]) != 0) : true;
            Core::Audio::AudioManager::Instance().SetMLSpatializationEnabled(enabled);
            return (int64_t)1;
        });
        vm.RegisterNative("Audio.MLSpatialization.Train", [](const std::vector<Value>&) -> Value {
            Core::Audio::AudioManager::Instance().TrainMLModel();
            return (int64_t)1;
        });
        vm.RegisterNative("Audio.MLSpatialization.SaveWeights", [](const std::vector<Value>& args) -> Value {
            if (args.empty()) return (int64_t)0;
            return (int64_t)(Core::Audio::AudioManager::Instance().SaveMLWeights(GetString(args[0]).c_str()) ? 1 : 0);
        });
        vm.RegisterNative("Audio.MLSpatialization.LoadWeights", [](const std::vector<Value>& args) -> Value {
            if (args.empty()) return (int64_t)0;
            return (int64_t)(Core::Audio::AudioManager::Instance().LoadMLWeights(GetString(args[0]).c_str()) ? 1 : 0);
        });

        // ========== Motion Graphics ==========
        // Note: UI::MotionGraphics::Animate doesn't exist, using AnimatedButton instead
        vm.RegisterNative("UI.MoGraph.AnimatedButton", [](const std::vector<Value>& args) -> Value {
            // Placeholder - would need proper AnimationClip setup
            return (int64_t)0;
        });

        // ========== Math Functions (Enhanced) ==========
        vm.RegisterNative("Math.Sin", [](const std::vector<Value>& args) -> Value {
            return (double)std::sin(args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Cos", [](const std::vector<Value>& args) -> Value {
            return (double)std::cos(args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Tan", [](const std::vector<Value>& args) -> Value {
            return (double)std::tan(args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Asin", [](const std::vector<Value>& args) -> Value {
            return (double)std::asin(args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Acos", [](const std::vector<Value>& args) -> Value {
            return (double)std::acos(args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Atan", [](const std::vector<Value>& args) -> Value {
            return (double)std::atan(args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Atan2", [](const std::vector<Value>& args) -> Value {
            return (double)std::atan2(args.size() > 1 ? GetFloat(args[1]) : 0.0f, args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Sinh", [](const std::vector<Value>& args) -> Value {
            return (double)std::sinh(args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Cosh", [](const std::vector<Value>& args) -> Value {
            return (double)std::cosh(args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Tanh", [](const std::vector<Value>& args) -> Value {
            return (double)std::tanh(args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Exp", [](const std::vector<Value>& args) -> Value {
            return (double)std::exp(args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Log", [](const std::vector<Value>& args) -> Value {
            return (double)std::log(args.size() > 0 ? GetFloat(args[0]) : 1.0f);
        });
        vm.RegisterNative("Math.Log10", [](const std::vector<Value>& args) -> Value {
            return (double)std::log10(args.size() > 0 ? GetFloat(args[0]) : 1.0f);
        });
        vm.RegisterNative("Math.Log2", [](const std::vector<Value>& args) -> Value {
            return (double)std::log2(args.size() > 0 ? GetFloat(args[0]) : 1.0f);
        });
        vm.RegisterNative("Math.Pow", [](const std::vector<Value>& args) -> Value {
            return (double)std::pow(args.size() > 0 ? GetFloat(args[0]) : 0.0f, args.size() > 1 ? GetFloat(args[1]) : 0.0f);
        });
        vm.RegisterNative("Math.Sqrt", [](const std::vector<Value>& args) -> Value {
            return (double)std::sqrt(args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Cbrt", [](const std::vector<Value>& args) -> Value {
            return (double)std::cbrt(args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Floor", [](const std::vector<Value>& args) -> Value {
            return (double)std::floor(args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Ceil", [](const std::vector<Value>& args) -> Value {
            return (double)std::ceil(args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Round", [](const std::vector<Value>& args) -> Value {
            return (double)std::round(args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Trunc", [](const std::vector<Value>& args) -> Value {
            return (double)std::trunc(args.size() > 0 ? GetFloat(args[0]) : 0.0f);
        });
        vm.RegisterNative("Math.Abs", [](const std::vector<Value>& args) -> Value {
            float val = args.size() > 0 ? GetFloat(args[0]) : 0.0f;
            return (double)std::abs(val);
        });
        vm.RegisterNative("Math.Min", [](const std::vector<Value>& args) -> Value {
            return (double)std::min(args.size() > 0 ? GetFloat(args[0]) : 0.0f, args.size() > 1 ? GetFloat(args[1]) : 0.0f);
        });
        vm.RegisterNative("Math.Max", [](const std::vector<Value>& args) -> Value {
            return (double)std::max(args.size() > 0 ? GetFloat(args[0]) : 0.0f, args.size() > 1 ? GetFloat(args[1]) : 0.0f);
        });
        vm.RegisterNative("Math.Clamp", [](const std::vector<Value>& args) -> Value {
            float val = args.size() > 0 ? GetFloat(args[0]) : 0.0f;
            float min = args.size() > 1 ? GetFloat(args[1]) : 0.0f;
            float max = args.size() > 2 ? GetFloat(args[2]) : 1.0f;
            return (double)std::clamp(val, min, max);
        });
        vm.RegisterNative("Math.Lerp", [](const std::vector<Value>& args) -> Value {
            float a = args.size() > 0 ? GetFloat(args[0]) : 0.0f;
            float b = args.size() > 1 ? GetFloat(args[1]) : 0.0f;
            float t = args.size() > 2 ? GetFloat(args[2]) : 0.0f;
            return (double)(a + t * (b - a));
        });
        vm.RegisterNative("Math.SmoothStep", [](const std::vector<Value>& args) -> Value {
            float edge0 = args.size() > 0 ? GetFloat(args[0]) : 0.0f;
            float edge1 = args.size() > 1 ? GetFloat(args[1]) : 1.0f;
            float x = args.size() > 2 ? GetFloat(args[2]) : 0.5f;
            float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return (double)(t * t * (3.0f - 2.0f * t));
        });
        vm.RegisterNative("Math.Sign", [](const std::vector<Value>& args) -> Value {
            float val = args.size() > 0 ? GetFloat(args[0]) : 0.0f;
            return (double)(val > 0.0f ? 1.0f : (val < 0.0f ? -1.0f : 0.0f));
        });
        vm.RegisterNative("Math.PI", [](const std::vector<Value>& args) -> Value {
            return (double)3.14159265358979323846;
        });
        vm.RegisterNative("Math.E", [](const std::vector<Value>& args) -> Value {
            return (double)2.71828182845904523536;
        });
        vm.RegisterNative("Math.TAU", [](const std::vector<Value>& args) -> Value {
            return (double)6.28318530717958647692;
        });
        vm.RegisterNative("Math.DEG2RAD", [](const std::vector<Value>& args) -> Value {
            return (double)0.01745329251994329577;
        });
        vm.RegisterNative("Math.RAD2DEG", [](const std::vector<Value>& args) -> Value {
            return (double)57.2957795130823208768;
        });
        vm.RegisterNative("Math.Range", [](const std::vector<Value>& args) -> Value {
            // Creates a range for iteration: Range(start, end)
            int64_t start = args.size() > 0 ? GetInt(args[0]) : 0;
            int64_t end = args.size() > 1 ? GetInt(args[1]) : 0;
            auto arr = std::make_shared<Array>();
            for (int64_t i = start; i < end; ++i) {
                arr->Push((int64_t)i);
            }
            return arr;
        });

        // ========== Physics Functions (Enhanced) ==========
        if (physicsSystem) {
            vm.RegisterNative("Physics.SetVelocity", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (int64_t)0;
                if (args.size() >= 4 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    rb.Velocity = Vec3(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3]));
                }
                return (int64_t)0;
            });
            vm.RegisterNative("Physics.GetVelocity", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return Vec3();
                if (args.size() > 0 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    const auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    return rb.Velocity;
                }
                return Vec3();
            });
            vm.RegisterNative("Physics.SetAngularVelocity", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return (int64_t)0;
                if (args.size() >= 4 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    rb.AngularVelocity = Vec3(GetFloat(args[1]), GetFloat(args[2]), GetFloat(args[3]));
                }
                return (int64_t)0;
            });
            vm.RegisterNative("Physics.GetAngularVelocity", [registry](const std::vector<Value>& args) -> Value {
                if (!registry) return Vec3();
                if (args.size() > 0 && registry->Has<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]))) {
                    const auto& rb = registry->Get<Physics::RigidBody>((ECS::EntityId)GetInt(args[0]));
                    return rb.AngularVelocity;
                }
                return Vec3();
            });
            vm.RegisterNative("Physics.Raycast", [physicsSystem, registry](const std::vector<Value>& args) -> Value {
                auto noHit = std::make_shared<Dictionary>();
                noHit->Set("hit", (int64_t)0);
                
                if (!physicsSystem || !registry) return noHit;
                if (args.size() < 6) return noHit; // Need origin (3), direction (3), maxDistance (1)
                
                // Get origin Vec3
                Vec3 origin;
                if (std::holds_alternative<Vec3>(args[0])) {
                    origin = std::get<Vec3>(args[0]);
                } else {
                    origin = Vec3(GetFloat(args[0]), GetFloat(args[1]), GetFloat(args[2]));
                }
                
                // Get direction Vec3
                Vec3 direction;
                int dirArgStart = std::holds_alternative<Vec3>(args[0]) ? 1 : 3;
                if (dirArgStart < args.size() && std::holds_alternative<Vec3>(args[dirArgStart])) {
                    direction = std::get<Vec3>(args[dirArgStart]);
                } else if (dirArgStart + 2 < args.size()) {
                    direction = Vec3(GetFloat(args[dirArgStart]), GetFloat(args[dirArgStart + 1]), GetFloat(args[dirArgStart + 2]));
                } else {
                    return noHit;
                }
                
                float maxDistance = dirArgStart + (std::holds_alternative<Vec3>(args[dirArgStart]) ? 1 : 3) < args.size() 
                    ? GetFloat(args[dirArgStart + (std::holds_alternative<Vec3>(args[dirArgStart]) ? 1 : 3)]) : 1000.0f;
                
                Physics::RaycastRequest rq{};
                rq.Origin = origin;
                rq.Direction = direction.Normalized();
                rq.MaxDistance = maxDistance;
                Physics::RaycastHit hit = physicsSystem->RaycastClosest(rq);

                if (hit.Hit) {
                    auto hitData = std::make_shared<Dictionary>();
                    hitData->Set("hit", (int64_t)1);
                    hitData->Set("position", hit.Point);
                    hitData->Set("normal", hit.Normal);
                    hitData->Set("distance", (double)hit.Distance);
                    hitData->Set("entityId", (int64_t)hit.Entity);
                    return hitData;
                }
                
                // No hit - return the noHit we created earlier
                return noHit;
            });
            
            vm.RegisterNative("Physics.OverlapSphere", [physicsSystem, registry](const std::vector<Value>& args) -> Value {
                if (!physicsSystem || !registry) return std::make_shared<Array>();
                if (args.size() < 4) return std::make_shared<Array>(); // Need center (3), radius (1)
                
                Vec3 center;
                if (std::holds_alternative<Vec3>(args[0])) {
                    center = std::get<Vec3>(args[0]);
                } else {
                    center = Vec3(GetFloat(args[0]), GetFloat(args[1]), GetFloat(args[2]));
                }
                
                float radius = std::holds_alternative<Vec3>(args[0]) ? GetFloat(args[1]) : GetFloat(args[3]);
                
                auto result = std::make_shared<Array>();
                
                const auto entities = physicsSystem->OverlapSphere(center, radius);
                for (const auto entityId : entities) {
                    result->Push((int64_t)entityId);
                }
                
                return result;
            });
            
            vm.RegisterNative("Physics.OverlapBox", [physicsSystem, registry](const std::vector<Value>& args) -> Value {
                if (!physicsSystem || !registry) return std::make_shared<Array>();
                if (args.size() < 6) return std::make_shared<Array>(); // Need center (3), size (3)
                
                Vec3 center;
                int centerArgEnd = std::holds_alternative<Vec3>(args[0]) ? 1 : 3;
                if (std::holds_alternative<Vec3>(args[0])) {
                    center = std::get<Vec3>(args[0]);
                } else {
                    center = Vec3(GetFloat(args[0]), GetFloat(args[1]), GetFloat(args[2]));
                }
                
                Vec3 size;
                if (centerArgEnd < args.size() && std::holds_alternative<Vec3>(args[centerArgEnd])) {
                    size = std::get<Vec3>(args[centerArgEnd]);
                } else if (centerArgEnd + 2 < args.size()) {
                    size = Vec3(GetFloat(args[centerArgEnd]), GetFloat(args[centerArgEnd + 1]), GetFloat(args[centerArgEnd + 2]));
                } else {
                    return std::make_shared<Array>();
                }
                
                auto result = std::make_shared<Array>();
                
                const Vec3 min(center.x - size.x * 0.5f, center.y - size.y * 0.5f, center.z - size.z * 0.5f);
                const Vec3 max(center.x + size.x * 0.5f, center.y + size.y * 0.5f, center.z + size.z * 0.5f);
                const auto entities = physicsSystem->OverlapAabb(min, max);
                for (const auto entityId : entities) {
                    result->Push((int64_t)entityId);
                }
                
                return result;
            });
            
            vm.RegisterNative("Physics.CreateJoint", [physicsSystem](const std::vector<Value>& args) -> Value {
                // Joint system not yet implemented in ReactPhysics3D bridge
                // Return 0 as placeholder - joints would need additional implementation
                return (int64_t)0;
            });
            
            vm.RegisterNative("Physics.RemoveJoint", [physicsSystem](const std::vector<Value>& args) -> Value {
                // Joint system not yet implemented
                return (int64_t)0;
            });
            // Note: Physics::SetGravity and GetGravity don't exist in PhysicsSystem
            // These would need to be implemented or accessed through ReactPhysics3D bridge
            vm.RegisterNative("Physics.SetGravity", [physicsSystem](const std::vector<Value>& args) -> Value {
                // Placeholder - would need to access ReactPhysics3D world
                return (int64_t)0;
            });
            vm.RegisterNative("Physics.GetGravity", [physicsSystem](const std::vector<Value>& args) -> Value {
                // Placeholder - would need to access ReactPhysics3D world
                return Vec3(0.0f, -9.81f, 0.0f); // Default gravity
            });
        }

        // ========== Profiler Functions ==========
        vm.RegisterNative("Profiler.BeginScope", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0) {
                Core::Profiler::Instance().BeginScope(GetString(args[0]).c_str());
            }
            return (int64_t)0;
        });
        vm.RegisterNative("Profiler.EndScope", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0) {
                Core::Profiler::Instance().EndScope(GetString(args[0]).c_str());
            }
            return (int64_t)0;
        });
        vm.RegisterNative("Profiler.IncrementCounter", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2) {
                Core::Profiler::Instance().IncrementCounter(GetString(args[0]).c_str(), GetInt(args[1]));
            }
            return (int64_t)0;
        });
        vm.RegisterNative("Profiler.SetCounter", [](const std::vector<Value>& args) -> Value {
            if (args.size() >= 2) {
                Core::Profiler::Instance().SetCounter(GetString(args[0]).c_str(), GetInt(args[1]));
            }
            return (int64_t)0;
        });
        vm.RegisterNative("Profiler.GetCounter", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0) {
                return (int64_t)Core::Profiler::Instance().GetCounter(GetString(args[0]).c_str());
            }
            return (int64_t)0;
        });
        vm.RegisterNative("Profiler.TrackMemoryAlloc", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0) {
                Core::Profiler::Instance().TrackMemoryAlloc((size_t)GetInt(args[0]));
            }
            return (int64_t)0;
        });
        vm.RegisterNative("Profiler.TrackMemoryFree", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0) {
                Core::Profiler::Instance().TrackMemoryFree((size_t)GetInt(args[0]));
            }
            return (int64_t)0;
        });
        vm.RegisterNative("Profiler.GetMemoryUsage", [](const std::vector<Value>& args) -> Value {
            return (int64_t)Core::Profiler::Instance().GetMemoryUsage();
        });
        vm.RegisterNative("Profiler.GetPeakMemory", [](const std::vector<Value>& args) -> Value {
            return (int64_t)Core::Profiler::Instance().GetPeakMemory();
        });
        vm.RegisterNative("Profiler.GetLastFrameStats", [](const std::vector<Value>& args) -> Value {
            // Returns a dictionary with frame stats
            auto dict = std::make_shared<Dictionary>();
            auto stats = Core::Profiler::Instance().GetLastFrameStats();
            dict->Set("FrameTime", Value((double)stats.FrameTime));
            dict->Set("FPS", Value((double)stats.FPS));
            dict->Set("MemoryUsage", Value((int64_t)stats.MemoryUsage));
            return dict;
        });
        vm.RegisterNative("Profiler.BeginFrame", [](const std::vector<Value>& args) -> Value {
            Core::Profiler::Instance().BeginFrame();
            return (int64_t)0;
        });
        vm.RegisterNative("Profiler.EndFrame", [](const std::vector<Value>& args) -> Value {
            Core::Profiler::Instance().EndFrame();
            return (int64_t)0;
        });
        vm.RegisterNative("Profiler.SetEnabled", [](const std::vector<Value>& args) -> Value {
            if (args.size() > 0) {
                Core::Profiler::Instance().SetEnabled(GetInt(args[0]) != 0);
            }
            return (int64_t)0;
        });
        vm.RegisterNative("Profiler.IsEnabled", [](const std::vector<Value>& args) -> Value {
            return (int64_t)(Core::Profiler::Instance().IsEnabled() ? 1 : 0);
        });
    }
}
