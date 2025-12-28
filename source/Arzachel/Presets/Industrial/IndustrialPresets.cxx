#include "IndustrialPresets.hxx"
#include "../../GeometryOps.hxx"
#include "../../Polyhedra.hxx"
#include "../../Seed.hxx"
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <cmath>

namespace Solstice::Arzachel {

namespace Math = Solstice::Math;

Generator<MeshData> Pipe(const Seed& SeedParam, float Length, float Radius) {
    return Generator<MeshData>([SeedParam, Length, Radius](const Seed& SeedParamInner) {
        Generator<MeshData> PipeMesh = Cylinder(Radius, Length, 16, SeedParamInner);
        PipeMesh = Transform(PipeMesh, Math::Matrix4::Translation(Math::Vec3(0, Length * 0.5f, 0)));
        return PipeMesh(SeedParamInner);
    });
}

Generator<MeshData> ControlPanel(const Seed& SeedParam, float Width) {
    return Generator<MeshData>([SeedParam, Width](const Seed& SeedParamInner) {
        float Height = 0.8f;
        float Depth = 0.3f;

        // Main panel
        Generator<MeshData> Panel = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Width, Height, Depth));
        Panel = Transform(Panel, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, 0)));

        // Buttons (procedural placement)
        int ButtonCount = static_cast<int>(Width * 2.0f);
        for (int i = 0; i < ButtonCount && i < 10; ++i) {
            float x = (i / static_cast<float>(ButtonCount - 1)) * Width - Width * 0.5f;
            float y = Height * (0.3f + (i % 3) * 0.2f);
            Generator<MeshData> Button = Cylinder(0.02f, 0.03f, 12, SeedParamInner.Derive(10 + i));
            Button = Transform(Button, Math::Matrix4::Translation(Math::Vec3(x, y, Depth * 0.51f)));
            Panel = Merge(Panel, Button);
        }

        return Panel(SeedParamInner);
    });
}

Generator<MeshData> Machinery(const Seed& SeedParam, int Type) {
    return Generator<MeshData>([SeedParam, Type](const Seed& SeedParamInner) {
        // Generic machinery - varies by type
        float Size = Type == 0 ? 1.0f : (Type == 1 ? 1.5f : 0.8f);
        
        // Main body
        Generator<MeshData> Body = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Size, Size * 0.8f, Size * 0.6f));
        Body = Transform(Body, Math::Matrix4::Translation(Math::Vec3(0, Size * 0.4f, 0)));

        // Control elements
        Generator<MeshData> ControlBox = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(Size * 0.3f, Size * 0.2f, Size * 0.2f));
        ControlBox = Transform(ControlBox, Math::Matrix4::Translation(Math::Vec3(Size * 0.4f, Size * 0.6f, 0)));
        Body = Merge(Body, ControlBox);

        return Body(SeedParamInner);
    });
}

Generator<MeshData> Vent(const Seed& SeedParam, float Size) {
    return Generator<MeshData>([SeedParam, Size](const Seed& SeedParamInner) {
        // Ventilation duct
        Generator<MeshData> Duct = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Size, Size * 0.3f, Size * 0.3f));
        Duct = Transform(Duct, Math::Matrix4::Translation(Math::Vec3(0, Size * 0.15f, 0)));

        // Grille
        int GrilleCount = 5;
        for (int i = 0; i < GrilleCount; ++i) {
            float x = (i / static_cast<float>(GrilleCount - 1)) * Size - Size * 0.5f;
            Generator<MeshData> Bar = Scale(Cube(1.0f, SeedParamInner.Derive(1 + i)), Math::Vec3(0.01f, Size * 0.3f, Size * 0.3f));
            Bar = Transform(Bar, Math::Matrix4::Translation(Math::Vec3(x, Size * 0.15f, 0)));
            Duct = Merge(Duct, Bar);
        }

        return Duct(SeedParamInner);
    });
}

Generator<MeshData> Terminal(const Seed& SeedParam, int Type) {
    return Generator<MeshData>([SeedParam, Type](const Seed& SeedParamInner) {
        // Base unit
        Generator<MeshData> Base = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.4f, 0.3f, 0.3f));
        Base = Transform(Base, Math::Matrix4::Translation(Math::Vec3(0, 0.15f, 0)));

        // Screen
        Generator<MeshData> Screen = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(0.35f, 0.25f, 0.02f));
        Screen = Transform(Screen, Math::Matrix4::Translation(Math::Vec3(0, 0.4f, 0.16f)));

        // Keyboard (if desktop terminal)
        if (Type == 0) {
            Generator<MeshData> Keyboard = Scale(Cube(1.0f, SeedParamInner.Derive(2)), Math::Vec3(0.3f, 0.05f, 0.15f));
            Keyboard = Transform(Keyboard, Math::Matrix4::Translation(Math::Vec3(0, 0.05f, 0.2f)));
            Base = Merge(Base, Keyboard);
        }

        Generator<MeshData> Result = Merge(Base, Screen);
        return Result(SeedParamInner);
    });
}

Generator<MeshData> Computer(const Seed& SeedParam, int Type) {
    return Generator<MeshData>([SeedParam, Type](const Seed& SeedParamInner) {
        // Tower case
        Generator<MeshData> Tower = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.2f, 0.4f, 0.3f));
        Tower = Transform(Tower, Math::Matrix4::Translation(Math::Vec3(0, 0.2f, 0)));

        // Front panel details
        Generator<MeshData> Panel = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(0.18f, 0.35f, 0.02f));
        Panel = Transform(Panel, Math::Matrix4::Translation(Math::Vec3(0, 0.2f, 0.16f)));
        Tower = Merge(Tower, Panel);

        return Tower(SeedParamInner);
    });
}

Generator<MeshData> Monitor(const Seed& SeedParam, float Size, int Type) {
    return Generator<MeshData>([SeedParam, Size, Type](const Seed& SeedParamInner) {
        // CRT monitor (early '00s style)
        float Width = 0.4f * Size;
        float Height = 0.3f * Size;
        float Depth = 0.4f * Size;

        // Screen (front)
        Generator<MeshData> Screen = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Width, Height, 0.05f));
        Screen = Transform(Screen, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, Depth * 0.48f)));

        // Body (curved back)
        Generator<MeshData> Body = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(Width * 1.1f, Height * 1.1f, Depth));
        Body = Transform(Body, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.55f, -Depth * 0.1f)));

        // Stand
        Generator<MeshData> Stand = Cylinder(0.05f, 0.15f, 12, SeedParamInner.Derive(2));
        Stand = Transform(Stand, Math::Matrix4::Translation(Math::Vec3(0, 0.075f, 0)));

        // Base
        Generator<MeshData> Base = Cylinder(0.08f, 0.02f, 12, SeedParamInner.Derive(3));
        Base = Transform(Base, Math::Matrix4::Translation(Math::Vec3(0, 0.01f, 0)));

        Generator<MeshData> Result = Merge(Screen, Body);
        Result = Merge(Result, Stand);
        Result = Merge(Result, Base);

        return Result(SeedParamInner);
    });
}

Generator<MeshData> Light(const Seed& SeedParam, int Type) {
    return Generator<MeshData>([SeedParam, Type](const Seed& SeedParamInner) {
        if (Type == 0) { // Ceiling light
            // Fixture
            Generator<MeshData> Fixture = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.3f, 0.05f, 0.3f));
            Fixture = Transform(Fixture, Math::Matrix4::Translation(Math::Vec3(0, 0.025f, 0)));

            // Bulb
            Generator<MeshData> Bulb = Sphere(0.1f, 12, SeedParamInner.Derive(1));
            Bulb = Transform(Bulb, Math::Matrix4::Translation(Math::Vec3(0, -0.05f, 0)));

            Generator<MeshData> Result = Merge(Fixture, Bulb);
            return Result(SeedParamInner);
        } else if (Type == 1) { // Wall light
            // Mount
            Generator<MeshData> Mount = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.15f, 0.15f, 0.05f));
            Mount = Transform(Mount, Math::Matrix4::Translation(Math::Vec3(0, 0, 0.025f)));

            // Shade
            Generator<MeshData> Shade = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(0.2f, 0.2f, 0.15f));
            Shade = Transform(Shade, Math::Matrix4::Translation(Math::Vec3(0, 0, 0.1f)));

            Generator<MeshData> Result = Merge(Mount, Shade);
            return Result(SeedParamInner);
        } else { // Desk lamp
            // Base
            Generator<MeshData> Base = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.1f, 0.02f, 0.1f));
            Base = Transform(Base, Math::Matrix4::Translation(Math::Vec3(0, 0.01f, 0)));

            // Arm
            Generator<MeshData> Arm = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(0.02f, 0.3f, 0.02f));
            Arm = Transform(Arm, Math::Matrix4::Translation(Math::Vec3(0, 0.17f, 0)));

            // Shade
            Generator<MeshData> Shade = Scale(Cube(1.0f, SeedParamInner.Derive(2)), Math::Vec3(0.15f, 0.1f, 0.15f));
            Shade = Transform(Shade, Math::Matrix4::Translation(Math::Vec3(0, 0.35f, 0)));

            Generator<MeshData> Result = Merge(Base, Arm);
            Result = Merge(Result, Shade);
            return Result(SeedParamInner);
        }
    });
}

} // namespace Solstice::Arzachel

