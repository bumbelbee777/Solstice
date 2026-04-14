#pragma once

#include "../../Solstice.hxx"
#include <imgui.h>
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <vector>

// Forward declaration
namespace Solstice::UI {
    class Sprite;
}

namespace Solstice::UI::Primitives {

// Shapes
SOLSTICE_API void DrawPolygon(ImDrawList* DrawList, const ImVec2& Center, float Radius, uint32_t Sides, ImU32 Color, float Thickness = 1.0f);
SOLSTICE_API void DrawPolygonFilled(ImDrawList* DrawList, const ImVec2& Center, float Radius, uint32_t Sides, ImU32 Color);
SOLSTICE_API void DrawStar(ImDrawList* DrawList, const ImVec2& Center, float OuterRadius, float InnerRadius, uint32_t Points, ImU32 Color, float Thickness = 1.0f);
SOLSTICE_API void DrawStarFilled(ImDrawList* DrawList, const ImVec2& Center, float OuterRadius, float InnerRadius, uint32_t Points, ImU32 Color);
SOLSTICE_API void DrawBezierQuadratic(ImDrawList* DrawList, const ImVec2& P0, const ImVec2& P1, const ImVec2& P2, ImU32 Color, float Thickness = 1.0f, int Segments = 20);
SOLSTICE_API void DrawBezierCubic(ImDrawList* DrawList, const ImVec2& P0, const ImVec2& P1, const ImVec2& P2, const ImVec2& P3, ImU32 Color, float Thickness = 1.0f, int Segments = 20);
SOLSTICE_API void DrawArc(ImDrawList* DrawList, const ImVec2& Center, float Radius, float StartAngle, float EndAngle, ImU32 Color, float Thickness = 1.0f, int Segments = 20);
SOLSTICE_API void DrawPie(ImDrawList* DrawList, const ImVec2& Center, float Radius, float StartAngle, float EndAngle, ImU32 Color, int Segments = 20);

// Gradients
SOLSTICE_API void DrawLinearGradientRect(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 ColorStart, ImU32 ColorEnd, float Angle = 0.0f);
SOLSTICE_API void DrawRadialGradientCircle(ImDrawList* DrawList, const ImVec2& Center, float Radius, ImU32 ColorCenter, ImU32 ColorEdge, int Segments = 32);
SOLSTICE_API void DrawConicGradientCircle(ImDrawList* DrawList, const ImVec2& Center, float Radius, ImU32 ColorStart, ImU32 ColorEnd, float StartAngle = 0.0f, int Segments = 32);

// Patterns
SOLSTICE_API void DrawStripePattern(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color1, ImU32 Color2, float StripeWidth, float Angle = 0.0f);
SOLSTICE_API void DrawDotPattern(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color, float DotSize, float Spacing);
SOLSTICE_API void DrawCheckerboardPattern(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color1, ImU32 Color2, float SquareSize);

// AE-style echoes: stacked translucent discs (screen-space additive-ish look via alpha falloff).
SOLSTICE_API void DrawEchoDiscTrail(ImDrawList* DrawList, const ImVec2& Center, float Radius, int EchoCount, ImU32 BaseColor);

// World-space rendering (3D viewport)
SOLSTICE_API void DrawPolygonWorldSpace(ImDrawList* DrawList, const Math::Vec3& WorldPos, float Radius, uint32_t Sides,
                           const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix,
                           int ScreenWidth, int ScreenHeight, ImU32 Color, float Thickness = 1.0f);
SOLSTICE_API void DrawSpriteWorldSpace(ImDrawList* DrawList, const Solstice::UI::Sprite& Sprite, const Math::Vec3& WorldPos,
                         const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix,
                         int ScreenWidth, int ScreenHeight);

} // namespace Solstice::UI::Primitives
