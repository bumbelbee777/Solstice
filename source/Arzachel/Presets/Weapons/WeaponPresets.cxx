#include "WeaponPresets.hxx"
#include "../../GeometryOps.hxx"
#include "../../Polyhedra.hxx"
#include "../../Seed.hxx"
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <cmath>

namespace Solstice::Arzachel {

namespace Math = Solstice::Math;

Generator<MeshData> Pistol(const Seed& SeedParam) {
    return Generator<MeshData>([SeedParam](const Seed& SeedParamInner) {
        // Grip
        Generator<MeshData> Grip = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.04f, 0.12f, 0.06f));
        Grip = Transform(Grip, Math::Matrix4::Translation(Math::Vec3(0, -0.06f, 0)));

        // Body/frame
        Generator<MeshData> Body = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(0.15f, 0.04f, 0.06f));
        Body = Transform(Body, Math::Matrix4::Translation(Math::Vec3(0.075f, 0.02f, 0)));

        // Barrel
        Generator<MeshData> Barrel = Cylinder(0.015f, 0.12f, 12, SeedParamInner.Derive(2));
        Barrel = Transform(Barrel, Math::Matrix4::Translation(Math::Vec3(0.15f, 0.02f, 0)));

        // Trigger guard
        Generator<MeshData> TriggerGuard = Torus(0.025f, 0.005f, 12, 8, SeedParamInner.Derive(3));
        TriggerGuard = Transform(TriggerGuard, Math::Matrix4::Translation(Math::Vec3(0.02f, -0.01f, 0)));
        TriggerGuard = Rotate(TriggerGuard, 1.57f, Math::Vec3(1, 0, 0));

        Generator<MeshData> Result = Merge(Grip, Body);
        Result = Merge(Result, Barrel);
        Result = Merge(Result, TriggerGuard);

        return Result(SeedParamInner);
    });
}

Generator<MeshData> Rifle(const Seed& SeedParam) {
    return Generator<MeshData>([SeedParam](const Seed& SeedParamInner) {
        // Stock
        Generator<MeshData> Stock = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.25f, 0.06f, 0.04f));
        Stock = Transform(Stock, Math::Matrix4::Translation(Math::Vec3(-0.125f, 0, 0)));

        // Body/receiver
        Generator<MeshData> Body = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(0.2f, 0.08f, 0.06f));
        Body = Transform(Body, Math::Matrix4::Translation(Math::Vec3(0.1f, 0, 0)));

        // Barrel
        Generator<MeshData> Barrel = Cylinder(0.02f, 0.4f, 12, SeedParamInner.Derive(2));
        Barrel = Transform(Barrel, Math::Matrix4::Translation(Math::Vec3(0.3f, 0, 0)));

        // Handguard
        Generator<MeshData> Handguard = Scale(Cube(1.0f, SeedParamInner.Derive(3)), Math::Vec3(0.15f, 0.05f, 0.05f));
        Handguard = Transform(Handguard, Math::Matrix4::Translation(Math::Vec3(0.25f, -0.02f, 0)));

        // Magazine
        Generator<MeshData> Magazine = Scale(Cube(1.0f, SeedParamInner.Derive(4)), Math::Vec3(0.04f, 0.12f, 0.04f));
        Magazine = Transform(Magazine, Math::Matrix4::Translation(Math::Vec3(0.1f, -0.1f, 0)));

        Generator<MeshData> Result = Merge(Stock, Body);
        Result = Merge(Result, Barrel);
        Result = Merge(Result, Handguard);
        Result = Merge(Result, Magazine);

        return Result(SeedParamInner);
    });
}

Generator<MeshData> Shotgun(const Seed& SeedParam) {
    return Generator<MeshData>([SeedParam](const Seed& SeedParamInner) {
        // Stock
        Generator<MeshData> Stock = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.3f, 0.08f, 0.05f));
        Stock = Transform(Stock, Math::Matrix4::Translation(Math::Vec3(-0.15f, 0, 0)));

        // Body/receiver
        Generator<MeshData> Body = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(0.15f, 0.1f, 0.08f));
        Body = Transform(Body, Math::Matrix4::Translation(Math::Vec3(0.075f, 0, 0)));

        // Barrel (wider for shotgun)
        Generator<MeshData> Barrel = Cylinder(0.025f, 0.35f, 12, SeedParamInner.Derive(2));
        Barrel = Transform(Barrel, Math::Matrix4::Translation(Math::Vec3(0.25f, 0, 0)));

        // Pump handle
        Generator<MeshData> Pump = Scale(Cube(1.0f, SeedParamInner.Derive(3)), Math::Vec3(0.12f, 0.03f, 0.03f));
        Pump = Transform(Pump, Math::Matrix4::Translation(Math::Vec3(0.2f, -0.05f, 0)));

        Generator<MeshData> Result = Merge(Stock, Body);
        Result = Merge(Result, Barrel);
        Result = Merge(Result, Pump);

        return Result(SeedParamInner);
    });
}

Generator<MeshData> MeleeWeapon(const Seed& SeedParam, int Type) {
    return Generator<MeshData>([SeedParam, Type](const Seed& SeedParamInner) {
        if (Type == 0) { // Knife
            // Blade
            Generator<MeshData> Blade = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.15f, 0.02f, 0.01f));
            Blade = Transform(Blade, Math::Matrix4::Translation(Math::Vec3(0.075f, 0, 0)));

            // Handle
            Generator<MeshData> Handle = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(0.08f, 0.02f, 0.02f));
            Handle = Transform(Handle, Math::Matrix4::Translation(Math::Vec3(-0.04f, 0, 0)));

            return Merge(Blade, Handle)(SeedParamInner);
        } else if (Type == 1) { // Crowbar
            // Shaft
            Generator<MeshData> Shaft = Cylinder(0.01f, 0.4f, 12, SeedParamInner);
            Shaft = Transform(Shaft, Math::Matrix4::Translation(Math::Vec3(0, 0.2f, 0)));

            // Curved end
            Generator<MeshData> CurvedEnd = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(0.08f, 0.02f, 0.02f));
            CurvedEnd = Transform(CurvedEnd, Math::Matrix4::Translation(Math::Vec3(0.04f, 0.4f, 0)));

            // Flat end
            Generator<MeshData> FlatEnd = Scale(Cube(1.0f, SeedParamInner.Derive(2)), Math::Vec3(0.06f, 0.02f, 0.02f));
            FlatEnd = Transform(FlatEnd, Math::Matrix4::Translation(Math::Vec3(-0.03f, 0, 0)));

            Generator<MeshData> Result = Merge(Shaft, CurvedEnd);
            Result = Merge(Result, FlatEnd);
            return Result(SeedParamInner);
        } else { // Baton
            // Handle
            Generator<MeshData> Handle = Cylinder(0.015f, 0.3f, 12, SeedParamInner);
            Handle = Transform(Handle, Math::Matrix4::Translation(Math::Vec3(0, 0.15f, 0)));

            return Handle(SeedParamInner);
        }
    });
}

} // namespace Solstice::Arzachel

