#include "BulletPresets.hxx"
#include "../../GeometryOps.hxx"
#include "../../Polyhedra.hxx"
#include "../../Seed.hxx"
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <cmath>

namespace Solstice::Arzachel {

namespace Math = Solstice::Math;

Generator<MeshData> Bullet(const Seed& SeedParam, float Caliber) {
    return Generator<MeshData>([SeedParam, Caliber](const Seed& SeedParamInner) {
        float Radius = (Caliber / 1000.0f) * 0.5f; // Convert mm to meters, then radius
        float Length = Radius * 3.0f; // Typical bullet length

        // Bullet tip (ogive)
        Generator<MeshData> Tip = Sphere(Radius, 12, SeedParamInner);
        Tip = Scale(Tip, Math::Vec3(1, 0.5f, 1));
        Tip = Transform(Tip, Math::Matrix4::Translation(Math::Vec3(0, Length * 0.3f, 0)));

        // Body (cylinder)
        Generator<MeshData> Body = Cylinder(Radius, Length * 0.7f, 12, SeedParamInner.Derive(1));
        Body = Transform(Body, Math::Matrix4::Translation(Math::Vec3(0, -Length * 0.15f, 0)));

        // Base
        Generator<MeshData> Base = Cylinder(Radius, Length * 0.1f, 12, SeedParamInner.Derive(2));
        Base = Transform(Base, Math::Matrix4::Translation(Math::Vec3(0, -Length * 0.45f, 0)));

        Generator<MeshData> Result = Merge(Tip, Body);
        Result = Merge(Result, Base);

        return Result(SeedParamInner);
    });
}

Generator<MeshData> Shell(const Seed& SeedParam, int Type) {
    return Generator<MeshData>([SeedParam, Type](const Seed& SeedParamInner) {
        float Radius = Type == 0 ? 0.018f : 0.012f; // Shotgun shell larger
        float Length = Type == 0 ? 0.07f : 0.05f;

        // Casing body
        Generator<MeshData> Casing = Cylinder(Radius, Length, 12, SeedParamInner);
        Casing = Transform(Casing, Math::Matrix4::Translation(Math::Vec3(0, Length * 0.5f, 0)));

        // Rim
        Generator<MeshData> Rim = Cylinder(Radius * 1.1f, Length * 0.1f, 12, SeedParamInner.Derive(1));
        Rim = Transform(Rim, Math::Matrix4::Translation(Math::Vec3(0, 0, 0)));

        Generator<MeshData> Result = Merge(Casing, Rim);
        return Result(SeedParamInner);
    });
}

Generator<MeshData> Projectile(const Seed& SeedParam, int Type) {
    return Generator<MeshData>([SeedParam, Type](const Seed& SeedParamInner) {
        if (Type == 0) { // Simple sphere
            return Sphere(0.01f, 12, SeedParamInner)(SeedParamInner);
        } else { // Arrow-like
            // Shaft
            Generator<MeshData> Shaft = Cylinder(0.005f, 0.1f, 12, SeedParamInner);
            Shaft = Transform(Shaft, Math::Matrix4::Translation(Math::Vec3(0, 0.05f, 0)));

            // Tip
            Generator<MeshData> Tip = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(0.01f, 0.01f, 0.02f));
            Tip = Transform(Tip, Math::Matrix4::Translation(Math::Vec3(0, 0.11f, 0)));

            Generator<MeshData> Result = Merge(Shaft, Tip);
            return Result(SeedParamInner);
        }
    });
}

} // namespace Solstice::Arzachel

