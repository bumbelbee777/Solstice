#include <UI/Motion/Primitives.hxx>
#include <UI/Motion/Sprite.hxx>
#include <UI/Viewport/ViewportUI.hxx>
#include <cmath>
#include <algorithm>

namespace Solstice::UI::Primitives {

// Helper function to convert angle to radians
static float ToRadians(float Degrees) {
    return Degrees * 3.14159265359f / 180.0f;
}

// Helper function to get color components
static void GetColorComponents(ImU32 Color, uint8_t& R, uint8_t& G, uint8_t& B, uint8_t& A) {
    R = (Color >> IM_COL32_R_SHIFT) & 0xFF;
    G = (Color >> IM_COL32_G_SHIFT) & 0xFF;
    B = (Color >> IM_COL32_B_SHIFT) & 0xFF;
    A = (Color >> IM_COL32_A_SHIFT) & 0xFF;
}

static ImU32 MakeColor(uint8_t R, uint8_t G, uint8_t B, uint8_t A) {
    return IM_COL32(R, G, B, A);
}

// Draw polygon
void DrawPolygon(ImDrawList* DrawList, const ImVec2& Center, float Radius, uint32_t Sides, ImU32 Color, float Thickness) {
    if (!DrawList || Sides < 3) return;

    const float angleStep = 2.0f * 3.14159265359f / static_cast<float>(Sides);
    std::vector<ImVec2> points;
    points.reserve(Sides);

    for (uint32_t i = 0; i < Sides; ++i) {
        float angle = static_cast<float>(i) * angleStep;
        points.push_back(ImVec2(Center.x + Radius * std::cos(angle),
                                Center.y + Radius * std::sin(angle)));
    }

    for (uint32_t i = 0; i < Sides; ++i) {
        DrawList->AddLine(points[i], points[(i + 1) % Sides], Color, Thickness);
    }
}

void DrawPolygonFilled(ImDrawList* DrawList, const ImVec2& Center, float Radius, uint32_t Sides, ImU32 Color) {
    if (!DrawList || Sides < 3) return;

    const float angleStep = 2.0f * 3.14159265359f / static_cast<float>(Sides);

    for (uint32_t i = 0; i < Sides; ++i) {
        float angle1 = static_cast<float>(i) * angleStep;
        float angle2 = static_cast<float>((i + 1) % Sides) * angleStep;

        ImVec2 p1(Center.x + Radius * std::cos(angle1), Center.y + Radius * std::sin(angle1));
        ImVec2 p2(Center.x + Radius * std::cos(angle2), Center.y + Radius * std::sin(angle2));

        DrawList->AddTriangleFilled(Center, p1, p2, Color);
    }
}

// Draw star
void DrawStar(ImDrawList* DrawList, const ImVec2& Center, float OuterRadius, float InnerRadius, uint32_t Points, ImU32 Color, float Thickness) {
    if (!DrawList || Points < 3) return;

    const float angleStep = 3.14159265359f / static_cast<float>(Points);
    std::vector<ImVec2> vertices;
    vertices.reserve(Points * 2);

    for (uint32_t i = 0; i < Points * 2; ++i) {
        float angle = static_cast<float>(i) * angleStep;
        float radius = (i % 2 == 0) ? OuterRadius : InnerRadius;
        vertices.push_back(ImVec2(Center.x + radius * std::cos(angle),
                                 Center.y + radius * std::sin(angle)));
    }

    for (size_t i = 0; i < vertices.size(); ++i) {
        DrawList->AddLine(vertices[i], vertices[(i + 1) % vertices.size()], Color, Thickness);
    }
}

void DrawStarFilled(ImDrawList* DrawList, const ImVec2& Center, float OuterRadius, float InnerRadius, uint32_t Points, ImU32 Color) {
    if (!DrawList || Points < 3) return;

    const float angleStep = 3.14159265359f / static_cast<float>(Points);
    std::vector<ImVec2> vertices;
    vertices.reserve(Points * 2);

    for (uint32_t i = 0; i < Points * 2; ++i) {
        float angle = static_cast<float>(i) * angleStep;
        float radius = (i % 2 == 0) ? OuterRadius : InnerRadius;
        vertices.push_back(ImVec2(Center.x + radius * std::cos(angle),
                                 Center.y + radius * std::sin(angle)));
    }

    // Triangulate from center
    for (size_t i = 0; i < vertices.size(); ++i) {
        DrawList->AddTriangleFilled(Center, vertices[i], vertices[(i + 1) % vertices.size()], Color);
    }
}

// Draw bezier curves
void DrawBezierQuadratic(ImDrawList* DrawList, const ImVec2& P0, const ImVec2& P1, const ImVec2& P2, ImU32 Color, float Thickness, int Segments) {
    if (!DrawList || Segments < 2) return;

    ImVec2 prevPoint = P0;
    for (int i = 1; i <= Segments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(Segments);
        float u = 1.0f - t;

        ImVec2 point;
        point.x = u * u * P0.x + 2.0f * u * t * P1.x + t * t * P2.x;
        point.y = u * u * P0.y + 2.0f * u * t * P1.y + t * t * P2.y;

        DrawList->AddLine(prevPoint, point, Color, Thickness);
        prevPoint = point;
    }
}

void DrawBezierCubic(ImDrawList* DrawList, const ImVec2& P0, const ImVec2& P1, const ImVec2& P2, const ImVec2& P3, ImU32 Color, float Thickness, int Segments) {
    if (!DrawList || Segments < 2) return;

    ImVec2 prevPoint = P0;
    for (int i = 1; i <= Segments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(Segments);
        float u = 1.0f - t;
        float uu = u * u;
        float uuu = uu * u;
        float tt = t * t;
        float ttt = tt * t;

        ImVec2 point;
        point.x = uuu * P0.x + 3.0f * uu * t * P1.x + 3.0f * u * tt * P2.x + ttt * P3.x;
        point.y = uuu * P0.y + 3.0f * uu * t * P1.y + 3.0f * u * tt * P2.y + ttt * P3.y;

        DrawList->AddLine(prevPoint, point, Color, Thickness);
        prevPoint = point;
    }
}

// Draw arc
void DrawArc(ImDrawList* DrawList, const ImVec2& Center, float Radius, float StartAngle, float EndAngle, ImU32 Color, float Thickness, int Segments) {
    if (!DrawList || Segments < 2) return;

    float startRad = ToRadians(StartAngle);
    float endRad = ToRadians(EndAngle);
    float angleStep = (endRad - startRad) / static_cast<float>(Segments);

    ImVec2 prevPoint(Center.x + Radius * std::cos(startRad), Center.y + Radius * std::sin(startRad));

    for (int i = 1; i <= Segments; ++i) {
        float angle = startRad + static_cast<float>(i) * angleStep;
        ImVec2 point(Center.x + Radius * std::cos(angle), Center.y + Radius * std::sin(angle));
        DrawList->AddLine(prevPoint, point, Color, Thickness);
        prevPoint = point;
    }
}

// Draw pie
void DrawPie(ImDrawList* DrawList, const ImVec2& Center, float Radius, float StartAngle, float EndAngle, ImU32 Color, int Segments) {
    if (!DrawList || Segments < 2) return;

    float startRad = ToRadians(StartAngle);
    float endRad = ToRadians(EndAngle);
    float angleStep = (endRad - startRad) / static_cast<float>(Segments);

    ImVec2 prevPoint(Center.x + Radius * std::cos(startRad), Center.y + Radius * std::sin(startRad));

    for (int i = 1; i <= Segments; ++i) {
        float angle = startRad + static_cast<float>(i) * angleStep;
        ImVec2 point(Center.x + Radius * std::cos(angle), Center.y + Radius * std::sin(angle));
        DrawList->AddTriangleFilled(Center, prevPoint, point, Color);
        prevPoint = point;
    }
}

// Draw linear gradient
void DrawLinearGradientRect(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 ColorStart, ImU32 ColorEnd, float Angle) {
    if (!DrawList) return;

    uint8_t r1, g1, b1, a1, r2, g2, b2, a2;
    GetColorComponents(ColorStart, r1, g1, b1, a1);
    GetColorComponents(ColorEnd, r2, g2, b2, a2);

    float angleRad = ToRadians(Angle);
    float cosA = std::cos(angleRad);
    float sinA = std::sin(angleRad);

    // Calculate gradient direction
    float width = Max.x - Min.x;
    float height = Max.y - Min.y;
    float centerX = (Min.x + Max.x) * 0.5f;
    float centerY = (Min.y + Max.y) * 0.5f;

    // Create gradient by subdividing into triangles
    const int subdivisions = 32;
    for (int i = 0; i < subdivisions; ++i) {
        float t1 = static_cast<float>(i) / static_cast<float>(subdivisions);
        float t2 = static_cast<float>(i + 1) / static_cast<float>(subdivisions);

        uint8_t r = static_cast<uint8_t>(r1 + (r2 - r1) * t1);
        uint8_t g = static_cast<uint8_t>(g1 + (g2 - g1) * t1);
        uint8_t b = static_cast<uint8_t>(b1 + (b2 - b1) * t1);
        uint8_t a = static_cast<uint8_t>(a1 + (a2 - a1) * t1);
        ImU32 color1 = MakeColor(r, g, b, a);

        r = static_cast<uint8_t>(r1 + (r2 - r1) * t2);
        g = static_cast<uint8_t>(g1 + (g2 - g1) * t2);
        b = static_cast<uint8_t>(b1 + (b2 - b1) * t2);
        a = static_cast<uint8_t>(a1 + (a2 - a1) * t2);
        ImU32 color2 = MakeColor(r, g, b, a);

        // Simplified gradient - draw as vertical strips
        ImVec2 p1(Min.x + width * t1, Min.y);
        ImVec2 p2(Min.x + width * t2, Min.y);
        ImVec2 p3(Min.x + width * t2, Max.y);
        ImVec2 p4(Min.x + width * t1, Max.y);

        DrawList->AddQuadFilled(p1, p2, p3, p4, color1);
    }
}

// Draw radial gradient
void DrawRadialGradientCircle(ImDrawList* DrawList, const ImVec2& Center, float Radius, ImU32 ColorCenter, ImU32 ColorEdge, int Segments) {
    if (!DrawList || Segments < 3) return;

    uint8_t rc, gc, bc, ac, re, ge, be, ae;
    GetColorComponents(ColorCenter, rc, gc, bc, ac);
    GetColorComponents(ColorEdge, re, ge, be, ae);

    const float angleStep = 2.0f * 3.14159265359f / static_cast<float>(Segments);
    const int rings = 16;

    for (int ring = 0; ring < rings; ++ring) {
        float r1 = Radius * static_cast<float>(ring) / static_cast<float>(rings);
        float r2 = Radius * static_cast<float>(ring + 1) / static_cast<float>(rings);

        float t = static_cast<float>(ring) / static_cast<float>(rings);
        uint8_t r = static_cast<uint8_t>(rc + (re - rc) * t);
        uint8_t g = static_cast<uint8_t>(gc + (ge - gc) * t);
        uint8_t b = static_cast<uint8_t>(bc + (be - bc) * t);
        uint8_t a = static_cast<uint8_t>(ac + (ae - ac) * t);
        ImU32 color = MakeColor(r, g, b, a);

        for (int i = 0; i < Segments; ++i) {
            float angle1 = static_cast<float>(i) * angleStep;
            float angle2 = static_cast<float>((i + 1) % Segments) * angleStep;

            ImVec2 p1(Center.x + r1 * std::cos(angle1), Center.y + r1 * std::sin(angle1));
            ImVec2 p2(Center.x + r2 * std::cos(angle1), Center.y + r2 * std::sin(angle1));
            ImVec2 p3(Center.x + r2 * std::cos(angle2), Center.y + r2 * std::sin(angle2));
            ImVec2 p4(Center.x + r1 * std::cos(angle2), Center.y + r1 * std::sin(angle2));

            DrawList->AddQuadFilled(p1, p2, p3, p4, color);
        }
    }
}

// Draw conic gradient
void DrawConicGradientCircle(ImDrawList* DrawList, const ImVec2& Center, float Radius, ImU32 ColorStart, ImU32 ColorEnd, float StartAngle, int Segments) {
    if (!DrawList || Segments < 3) return;

    uint8_t rs, gs, bs, as, re, ge, be, ae;
    GetColorComponents(ColorStart, rs, gs, bs, as);
    GetColorComponents(ColorEnd, re, ge, be, ae);

    const float angleStep = 2.0f * 3.14159265359f / static_cast<float>(Segments);
    float startRad = ToRadians(StartAngle);

    for (int i = 0; i < Segments; ++i) {
        float angle1 = startRad + static_cast<float>(i) * angleStep;
        float angle2 = startRad + static_cast<float>(i + 1) * angleStep;

        float t = static_cast<float>(i) / static_cast<float>(Segments);
        uint8_t r = static_cast<uint8_t>(rs + (re - rs) * t);
        uint8_t g = static_cast<uint8_t>(gs + (ge - gs) * t);
        uint8_t b = static_cast<uint8_t>(bs + (be - bs) * t);
        uint8_t a = static_cast<uint8_t>(as + (ae - as) * t);
        ImU32 color = MakeColor(r, g, b, a);

        ImVec2 p1(Center.x + Radius * std::cos(angle1), Center.y + Radius * std::sin(angle1));
        ImVec2 p2(Center.x + Radius * std::cos(angle2), Center.y + Radius * std::sin(angle2));

        DrawList->AddTriangleFilled(Center, p1, p2, color);
    }
}

// Draw stripe pattern
void DrawStripePattern(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color1, ImU32 Color2, float StripeWidth, float Angle) {
    if (!DrawList) return;

    float width = Max.x - Min.x;
    float height = Max.y - Min.y;
    float angleRad = ToRadians(Angle);

    // Simplified: draw horizontal stripes if angle is 0
    if (std::abs(Angle) < 0.01f) {
        float y = Min.y;
        bool useColor1 = true;
        while (y < Max.y) {
            ImVec2 p1(Min.x, y);
            ImVec2 p2(Max.x, y);
            ImVec2 p3(Max.x, std::min(y + StripeWidth, Max.y));
            ImVec2 p4(Min.x, std::min(y + StripeWidth, Max.y));

            DrawList->AddQuadFilled(p1, p2, p3, p4, useColor1 ? Color1 : Color2);
            y += StripeWidth;
            useColor1 = !useColor1;
        }
    } else {
        // For angled stripes, use a simpler approximation
        DrawList->AddRectFilled(Min, Max, Color1);
        // Add diagonal lines for pattern
        for (float y = Min.y; y < Max.y; y += StripeWidth * 2.0f) {
            DrawList->AddLine(ImVec2(Min.x, y), ImVec2(Max.x, y + height * std::tan(angleRad)), Color2, StripeWidth);
        }
    }
}

// Draw dot pattern
void DrawDotPattern(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color, float DotSize, float Spacing) {
    if (!DrawList) return;

    for (float y = Min.y; y < Max.y; y += Spacing) {
        for (float x = Min.x; x < Max.x; x += Spacing) {
            DrawList->AddCircleFilled(ImVec2(x, y), DotSize * 0.5f, Color);
        }
    }
}

// Draw checkerboard pattern
void DrawCheckerboardPattern(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color1, ImU32 Color2, float SquareSize) {
    if (!DrawList) return;

    float width = Max.x - Min.x;
    float height = Max.y - Min.y;

    int cols = static_cast<int>(width / SquareSize) + 1;
    int rows = static_cast<int>(height / SquareSize) + 1;

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            bool useColor1 = (row + col) % 2 == 0;
            ImVec2 p1(Min.x + col * SquareSize, Min.y + row * SquareSize);
            ImVec2 p2(std::min(Min.x + (col + 1) * SquareSize, Max.x), Min.y + row * SquareSize);
            ImVec2 p3(std::min(Min.x + (col + 1) * SquareSize, Max.x), std::min(Min.y + (row + 1) * SquareSize, Max.y));
            ImVec2 p4(Min.x + col * SquareSize, std::min(Min.y + (row + 1) * SquareSize, Max.y));

            DrawList->AddQuadFilled(p1, p2, p3, p4, useColor1 ? Color1 : Color2);
        }
    }
}

void DrawEchoDiscTrail(ImDrawList* DrawList, const ImVec2& Center, float Radius, int EchoCount, ImU32 BaseColor) {
    if (!DrawList || EchoCount < 1 || Radius <= 0.0f) {
        return;
    }
    uint8_t br = (BaseColor >> IM_COL32_R_SHIFT) & 0xFF;
    uint8_t bg = (BaseColor >> IM_COL32_G_SHIFT) & 0xFF;
    uint8_t bb = (BaseColor >> IM_COL32_B_SHIFT) & 0xFF;
    uint8_t ba = (BaseColor >> IM_COL32_A_SHIFT) & 0xFF;
    for (int i = EchoCount - 1; i >= 0; --i) {
        const float t = static_cast<float>(i + 1) / static_cast<float>(EchoCount + 2);
        const float r = Radius * (0.55f + 0.45f * t);
        const uint8_t a = static_cast<uint8_t>(static_cast<float>(ba) * t * 0.55f);
        const ImU32 col = IM_COL32(br, bg, bb, a);
        DrawList->AddCircleFilled(Center, r, col, 32);
    }
    DrawList->AddCircleFilled(Center, Radius * 0.52f,
                              IM_COL32(br, bg, bb, static_cast<uint8_t>(static_cast<float>(ba) * 0.95f)), 32);
}

// World-space rendering
void DrawPolygonWorldSpace(ImDrawList* DrawList, const Math::Vec3& WorldPos, float Radius, uint32_t Sides,
                           const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix,
                           int ScreenWidth, int ScreenHeight, ImU32 Color, float Thickness) {
    if (!DrawList || Sides < 3) return;

    // Project center to screen
    Math::Vec2 screenCenter = ViewportUI::ProjectToScreen(WorldPos, ViewMatrix, ProjectionMatrix, ScreenWidth, ScreenHeight);

    // Draw polygon in screen space
    DrawPolygon(DrawList, ImVec2(screenCenter.x, screenCenter.y), Radius, Sides, Color, Thickness);
}

void DrawSpriteWorldSpace(ImDrawList* DrawList, const Solstice::UI::Sprite& Sprite, const Math::Vec3& WorldPos,
                         const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix,
                         int ScreenWidth, int ScreenHeight) {
    if (!DrawList) return;

    // Use sprite's world-space rendering
    const_cast<Solstice::UI::Sprite&>(Sprite).RenderWorldSpace(WorldPos, ViewMatrix, ProjectionMatrix, ScreenWidth, ScreenHeight, DrawList);
}

} // namespace Solstice::UI::Primitives
