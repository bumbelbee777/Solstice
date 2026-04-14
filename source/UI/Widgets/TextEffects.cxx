#include <UI/Widgets/TextEffects.hxx>
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>

namespace Solstice::UI::TextEffects {

    void ShakeText(const std::string& Text, float Time, float Intensity, float Speed) {
        ImVec2 basePos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Calculate random offsets for shake effect
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        
        // Use time-based pseudo-random for consistent shake
        float seed = Time * Speed * 1000.0f;
        offsetX = (sin(seed * 7.13f) + cos(seed * 3.17f)) * Intensity;
        offsetY = (cos(seed * 5.23f) + sin(seed * 11.37f)) * Intensity;
        
        ImVec2 pos = ImVec2(basePos.x + offsetX, basePos.y + offsetY);
        
        // Render text at offset position
        drawList->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), Text.c_str());
        
        // Advance cursor to maintain layout
        ImVec2 textSize = ImGui::CalcTextSize(Text.c_str());
        ImGui::Dummy(textSize);
    }

    void BounceText(const std::string& Text, float Time, float Amplitude, float Speed) {
        ImVec2 basePos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Elastic bounce using absolute value of sine
        float bounce = std::abs(sin(Time * Speed * 3.14159f));
        float offsetY = -Amplitude * (1.0f - bounce);
        
        ImVec2 pos = ImVec2(basePos.x, basePos.y + offsetY);
        
        // Render text at bounced position
        drawList->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), Text.c_str());
        
        // Advance cursor
        ImVec2 textSize = ImGui::CalcTextSize(Text.c_str());
        ImGui::Dummy(textSize);
    }

    void RotateText(const std::string& Text, float Time, float Speed, bool Oscillate) {
        ImVec2 basePos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 textSize = ImGui::CalcTextSize(Text.c_str());
        
        float angle = Time * Speed;
        if (Oscillate) {
            // Oscillate between -45 and +45 degrees
            angle = sin(Time * Speed) * 0.785398f; // 45 degrees in radians
        } else {
            // Full rotation
            angle = fmod(angle, 6.28318f); // 2*PI
        }
        
        // Calculate center of text
        ImVec2 center = ImVec2(basePos.x + textSize.x * 0.5f, basePos.y + textSize.y * 0.5f);
        
        // For rotation, we'll use a simplified approach with character-by-character rotation
        // Full rotation requires more complex matrix math, so we'll do per-character offset
        float cosA = cos(angle);
        float sinA = sin(angle);
        
        // Render each character with rotation offset
        ImVec2 charPos = basePos;
        for (size_t i = 0; i < Text.length(); ++i) {
            char buf[2] = { Text[i], 0 };
            ImVec2 charSize = ImGui::CalcTextSize(buf);
            
            // Simple rotation approximation
            float offsetX = (charSize.x * 0.5f) * (cosA - 1.0f);
            float offsetY = (charSize.y * 0.5f) * (sinA);
            
            ImVec2 rotatedPos = ImVec2(charPos.x + offsetX, charPos.y + offsetY);
            drawList->AddText(rotatedPos, ImGui::GetColorU32(ImGuiCol_Text), buf);
            
            charPos.x += charSize.x;
        }
        
        // Advance cursor
        ImGui::Dummy(textSize);
    }

    void ScaleText(const std::string& Text, float Time, float MinScale, float MaxScale, float Speed) {
        ImVec2 basePos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Pulsing scale using sine wave
        float scale = MinScale + (MaxScale - MinScale) * (sin(Time * Speed * 3.14159f) * 0.5f + 0.5f);
        
        ImVec2 textSize = ImGui::CalcTextSize(Text.c_str());
        ImVec2 scaledSize = ImVec2(textSize.x * scale, textSize.y * scale);
        
        // Center the scaled text
        ImVec2 offset = ImVec2((textSize.x - scaledSize.x) * 0.5f, (textSize.y - scaledSize.y) * 0.5f);
        ImVec2 pos = ImVec2(basePos.x + offset.x, basePos.y + offset.y);
        
        // Push font scale
        ImGui::PushFont(nullptr); // Use current font
        ImGui::SetWindowFontScale(scale);
        
        // Render text
        drawList->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), Text.c_str());
        
        // Restore font scale
        ImGui::SetWindowFontScale(1.0f);
        
        // Advance cursor (use original size for layout)
        ImGui::Dummy(textSize);
    }

    void WaveText(const std::string& Text, float Time, float Amplitude, float Speed, float Frequency) {
        ImVec2 basePos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        ImVec2 charPos = basePos;
        for (size_t i = 0; i < Text.length(); ++i) {
            char buf[2] = { Text[i], 0 };
            ImVec2 charSize = ImGui::CalcTextSize(buf);
            
            // Calculate wave offset for this character
            float wavePhase = (float)i * Frequency + Time * Speed;
            float offsetY = sin(wavePhase * 3.14159f) * Amplitude;
            
            ImVec2 wavePos = ImVec2(charPos.x, basePos.y + offsetY);
            drawList->AddText(wavePos, ImGui::GetColorU32(ImGuiCol_Text), buf);
            
            charPos.x += charSize.x;
        }
        
        // Advance cursor
        ImVec2 textSize = ImGui::CalcTextSize(Text.c_str());
        ImGui::Dummy(ImVec2(textSize.x, textSize.y + Amplitude * 2.0f));
    }

    void GlowText(const std::string& Text, const ImVec4& Color, float Time, float Intensity, float Speed) {
        ImVec2 basePos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Animated intensity if time is provided
        float glowIntensity = Intensity;
        if (Time > 0.0f) {
            glowIntensity = Intensity * (sin(Time * Speed * 3.14159f) * 0.5f + 0.5f);
        }
        
        ImVec2 textSize = ImGui::CalcTextSize(Text.c_str());
        ImU32 baseColor = ImGui::ColorConvertFloat4ToU32(Color);
        
        // Draw glow effect by rendering multiple offset copies with decreasing opacity
        int glowLayers = 8;
        for (int i = glowLayers; i > 0; --i) {
            float layerIntensity = (float)i / (float)glowLayers * glowIntensity;
            float offset = (float)(glowLayers - i) * 0.5f;
            
            // Extract RGB from color and apply intensity
            ImVec4 glowColor = Color;
            glowColor.w *= layerIntensity;
            ImU32 glowColorU32 = ImGui::ColorConvertFloat4ToU32(glowColor);
            
            // Draw glow in 8 directions (simplified to 4 for performance)
            drawList->AddText(ImVec2(basePos.x - offset, basePos.y), glowColorU32, Text.c_str());
            drawList->AddText(ImVec2(basePos.x + offset, basePos.y), glowColorU32, Text.c_str());
            drawList->AddText(ImVec2(basePos.x, basePos.y - offset), glowColorU32, Text.c_str());
            drawList->AddText(ImVec2(basePos.x, basePos.y + offset), glowColorU32, Text.c_str());
        }
        
        // Draw main text on top
        drawList->AddText(basePos, baseColor, Text.c_str());
        
        // Advance cursor
        ImGui::Dummy(textSize);
    }

} // namespace Solstice::UI::TextEffects
