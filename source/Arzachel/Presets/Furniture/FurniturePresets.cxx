#include "FurniturePresets.hxx"
#include "../../GeometryOps.hxx"
#include "../../Polyhedra.hxx"
#include "../../Seed.hxx"
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <cmath>

namespace Solstice::Arzachel {

namespace Math = Solstice::Math;

Generator<MeshData> Desk(const Seed& SeedParam, float Width, float Depth) {
    return Generator<MeshData>([SeedParam, Width, Depth](const Seed& SeedParamInner) {
        float Height = 0.75f;
        float Thickness = 0.03f;

        // Top
        Generator<MeshData> Top = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Width, Thickness, Depth));
        Top = Transform(Top, Math::Matrix4::Translation(Math::Vec3(0, Height, 0)));

        // Legs
        float LegThickness = 0.04f;
        Generator<MeshData> Leg1 = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(LegThickness, Height, LegThickness));
        Leg1 = Transform(Leg1, Math::Matrix4::Translation(Math::Vec3(-Width * 0.45f, Height * 0.5f, -Depth * 0.45f)));

        Generator<MeshData> Leg2 = Scale(Cube(1.0f, SeedParamInner.Derive(2)), Math::Vec3(LegThickness, Height, LegThickness));
        Leg2 = Transform(Leg2, Math::Matrix4::Translation(Math::Vec3(Width * 0.45f, Height * 0.5f, -Depth * 0.45f)));

        Generator<MeshData> Leg3 = Scale(Cube(1.0f, SeedParamInner.Derive(3)), Math::Vec3(LegThickness, Height, LegThickness));
        Leg3 = Transform(Leg3, Math::Matrix4::Translation(Math::Vec3(-Width * 0.45f, Height * 0.5f, Depth * 0.45f)));

        Generator<MeshData> Leg4 = Scale(Cube(1.0f, SeedParamInner.Derive(4)), Math::Vec3(LegThickness, Height, LegThickness));
        Leg4 = Transform(Leg4, Math::Matrix4::Translation(Math::Vec3(Width * 0.45f, Height * 0.5f, Depth * 0.45f)));

        Generator<MeshData> Result = Merge(Top, Leg1);
        Result = Merge(Result, Leg2);
        Result = Merge(Result, Leg3);
        Result = Merge(Result, Leg4);

        return Result(SeedParamInner);
    });
}

Generator<MeshData> Chair(const Seed& SeedParam, int Type) {
    return Generator<MeshData>([SeedParam, Type](const Seed& SeedParamInner) {
        if (Type == 0) { // Office chair
            float SeatHeight = 0.45f;
            float BackHeight = 0.4f;

            // Seat
            Generator<MeshData> Seat = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.4f, 0.03f, 0.4f));
            Seat = Transform(Seat, Math::Matrix4::Translation(Math::Vec3(0, SeatHeight, 0)));

            // Back
            Generator<MeshData> Back = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(0.4f, BackHeight, 0.03f));
            Back = Transform(Back, Math::Matrix4::Translation(Math::Vec3(0, SeatHeight + BackHeight * 0.5f, -0.18f)));

            // Legs (5-star base)
            float LegLength = 0.3f;
            for (int i = 0; i < 5; ++i) {
                float angle = (i / 5.0f) * 2.0f * 3.14159f;
                Generator<MeshData> Leg = Scale(Cube(1.0f, SeedParamInner.Derive(2 + i)), Math::Vec3(LegLength, 0.02f, 0.02f));
                Leg = Transform(Leg, Math::Matrix4::Translation(Math::Vec3(
                    std::cos(angle) * LegLength * 0.5f,
                    SeatHeight - 0.2f,
                    std::sin(angle) * LegLength * 0.5f
                )));
                Seat = Merge(Seat, Leg);
            }

            Generator<MeshData> Result = Merge(Seat, Back);
            return Result(SeedParamInner);
        } else { // Stool
            float Height = 0.45f;
            // Seat
            Generator<MeshData> Seat = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.3f, 0.03f, 0.3f));
            Seat = Transform(Seat, Math::Matrix4::Translation(Math::Vec3(0, Height, 0)));

            // Single leg
            Generator<MeshData> Leg = Cylinder(0.02f, Height, 12, SeedParamInner.Derive(1));
            Leg = Transform(Leg, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, 0)));

            Generator<MeshData> Result = Merge(Seat, Leg);
            return Result(SeedParamInner);
        }
    });
}

Generator<MeshData> Table(const Seed& SeedParam, float Size) {
    return Generator<MeshData>([SeedParam, Size](const Seed& SeedParamInner) {
        float Height = 0.75f;
        float Thickness = 0.03f;

        // Top
        Generator<MeshData> Top = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Size, Thickness, Size));
        Top = Transform(Top, Math::Matrix4::Translation(Math::Vec3(0, Height, 0)));

        // Single center leg
        Generator<MeshData> Leg = Cylinder(0.05f, Height, 12, SeedParamInner.Derive(1));
        Leg = Transform(Leg, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, 0)));

        Generator<MeshData> Result = Merge(Top, Leg);
        return Result(SeedParamInner);
    });
}

Generator<MeshData> Cabinet(const Seed& SeedParam, float Size) {
    return Generator<MeshData>([SeedParam, Size](const Seed& SeedParamInner) {
        float Width = Size;
        float Depth = Size * 0.6f;
        float Height = Size * 1.2f;

        // Main body
        Generator<MeshData> Body = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Width, Height, Depth));
        Body = Transform(Body, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, 0)));

        // Door
        Generator<MeshData> Door = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(Width * 0.48f, Height * 0.9f, 0.02f));
        Door = Transform(Door, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.45f, Depth * 0.51f)));

        Generator<MeshData> Result = Merge(Body, Door);
        return Result(SeedParamInner);
    });
}

Generator<MeshData> FilingCabinet(const Seed& SeedParam) {
    return Generator<MeshData>([SeedParam](const Seed& SeedParamInner) {
        float Width = 0.4f;
        float Depth = 0.5f;
        float Height = 1.2f;

        // Main body
        Generator<MeshData> Body = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Width, Height, Depth));
        Body = Transform(Body, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, 0)));

        // Drawers (2)
        for (int i = 0; i < 2; ++i) {
            Generator<MeshData> Drawer = Scale(Cube(1.0f, SeedParamInner.Derive(1 + i)), Math::Vec3(Width * 0.9f, Height * 0.45f, Depth * 0.48f));
            Drawer = Transform(Drawer, Math::Matrix4::Translation(Math::Vec3(0, Height * (0.25f + i * 0.5f), Depth * 0.51f)));
            Body = Merge(Body, Drawer);
        }

        return Body(SeedParamInner);
    });
}

} // namespace Solstice::Arzachel

