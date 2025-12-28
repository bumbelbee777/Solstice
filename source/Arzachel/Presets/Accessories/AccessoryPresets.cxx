#include "AccessoryPresets.hxx"
#include "../../GeometryOps.hxx"
#include "../../Polyhedra.hxx"
#include "../../Seed.hxx"
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <cmath>

namespace Solstice::Arzachel {

namespace Math = Solstice::Math;

Generator<MeshData> Backpack(const Seed& SeedParam, float Size) {
    return Generator<MeshData>([SeedParam, Size](const Seed& SeedParamInner) {
        // Main body
        Generator<MeshData> Body = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.4f * Size, 0.5f * Size, 0.3f * Size));
        Body = Transform(Body, Math::Matrix4::Translation(Math::Vec3(0, 0.25f * Size, 0)));

        // Straps
        Generator<MeshData> LeftStrap = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(0.05f * Size, 0.3f * Size, 0.05f * Size));
        LeftStrap = Transform(LeftStrap, Math::Matrix4::Translation(Math::Vec3(-0.15f * Size, 0.4f * Size, 0)));

        Generator<MeshData> RightStrap = Scale(Cube(1.0f, SeedParamInner.Derive(2)), Math::Vec3(0.05f * Size, 0.3f * Size, 0.05f * Size));
        RightStrap = Transform(RightStrap, Math::Matrix4::Translation(Math::Vec3(0.15f * Size, 0.4f * Size, 0)));

        // Pockets
        Generator<MeshData> FrontPocket = Scale(Cube(1.0f, SeedParamInner.Derive(3)), Math::Vec3(0.3f * Size, 0.2f * Size, 0.05f * Size));
        FrontPocket = Transform(FrontPocket, Math::Matrix4::Translation(Math::Vec3(0, 0.25f * Size, 0.18f * Size)));

        Generator<MeshData> Result = Merge(Body, LeftStrap);
        Result = Merge(Result, RightStrap);
        Result = Merge(Result, FrontPocket);

        return Result(SeedParamInner);
    });
}

Generator<MeshData> Helmet(const Seed& SeedParam, int Type) {
    return Generator<MeshData>([SeedParam, Type](const Seed& SeedParamInner) {
        // Base helmet (safety or combat)
        float Radius = Type == 1 ? 0.25f : 0.22f; // Combat helmet slightly larger
        Generator<MeshData> HelmetBase = Sphere(Radius, 12, SeedParamInner);
        
        // Flatten bottom
        HelmetBase = Scale(HelmetBase, Math::Vec3(1, 0.7f, 1));
        HelmetBase = Transform(HelmetBase, Math::Matrix4::Translation(Math::Vec3(0, Radius * 0.3f, 0)));

        // Visor for safety helmet
        Generator<MeshData> Result = HelmetBase;
        if (Type == 0) {
            Generator<MeshData> Visor = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(0.4f, 0.15f, 0.05f));
            Visor = Transform(Visor, Math::Matrix4::Translation(Math::Vec3(0, Radius * 0.2f, Radius * 0.4f)));
            Result = Merge(Result, Visor);
        }

        return Result(SeedParamInner);
    });
}

Generator<MeshData> Goggles(const Seed& SeedParam) {
    return Generator<MeshData>([SeedParam](const Seed& SeedParamInner) {
        // Frame
        Generator<MeshData> Frame = Torus(0.12f, 0.01f, 16, 8, SeedParamInner);
        Frame = Transform(Frame, Math::Matrix4::Translation(Math::Vec3(0, 0, 0)));

        // Lenses (slightly inset)
        Generator<MeshData> LeftLens = Cylinder(0.1f, 0.02f, 16, SeedParamInner.Derive(1));
        LeftLens = Transform(LeftLens, Math::Matrix4::Translation(Math::Vec3(-0.08f, 0, 0)));

        Generator<MeshData> RightLens = Cylinder(0.1f, 0.02f, 16, SeedParamInner.Derive(2));
        RightLens = Transform(RightLens, Math::Matrix4::Translation(Math::Vec3(0.08f, 0, 0)));

        // Strap connectors
        Generator<MeshData> LeftConnector = Scale(Cube(1.0f, SeedParamInner.Derive(3)), Math::Vec3(0.02f, 0.02f, 0.05f));
        LeftConnector = Transform(LeftConnector, Math::Matrix4::Translation(Math::Vec3(-0.15f, 0, 0)));

        Generator<MeshData> RightConnector = Scale(Cube(1.0f, SeedParamInner.Derive(4)), Math::Vec3(0.02f, 0.02f, 0.05f));
        RightConnector = Transform(RightConnector, Math::Matrix4::Translation(Math::Vec3(0.15f, 0, 0)));

        Generator<MeshData> Result = Merge(Frame, LeftLens);
        Result = Merge(Result, RightLens);
        Result = Merge(Result, LeftConnector);
        Result = Merge(Result, RightConnector);

        return Result(SeedParamInner);
    });
}

Generator<MeshData> GasMask(const Seed& SeedParam) {
    return Generator<MeshData>([SeedParam](const Seed& SeedParamInner) {
        // Face piece
        Generator<MeshData> FacePiece = Sphere(0.15f, 12, SeedParamInner);
        FacePiece = Scale(FacePiece, Math::Vec3(1, 0.8f, 0.9f));
        FacePiece = Transform(FacePiece, Math::Matrix4::Translation(Math::Vec3(0, 0, 0.05f)));

        // Eye lenses
        Generator<MeshData> LeftEye = Cylinder(0.04f, 0.02f, 12, SeedParamInner.Derive(1));
        LeftEye = Transform(LeftEye, Math::Matrix4::Translation(Math::Vec3(-0.05f, 0.03f, 0.12f)));

        Generator<MeshData> RightEye = Cylinder(0.04f, 0.02f, 12, SeedParamInner.Derive(2));
        RightEye = Transform(RightEye, Math::Matrix4::Translation(Math::Vec3(0.05f, 0.03f, 0.12f)));

        // Filter canister
        Generator<MeshData> Filter = Cylinder(0.04f, 0.08f, 12, SeedParamInner.Derive(3));
        Filter = Transform(Filter, Math::Matrix4::Translation(Math::Vec3(0.08f, -0.05f, 0.1f)));

        // Strap
        Generator<MeshData> Strap = Scale(Cube(1.0f, SeedParamInner.Derive(4)), Math::Vec3(0.3f, 0.02f, 0.02f));
        Strap = Transform(Strap, Math::Matrix4::Translation(Math::Vec3(0, -0.1f, 0)));

        Generator<MeshData> Result = Merge(FacePiece, LeftEye);
        Result = Merge(Result, RightEye);
        Result = Merge(Result, Filter);
        Result = Merge(Result, Strap);

        return Result(SeedParamInner);
    });
}

} // namespace Solstice::Arzachel

