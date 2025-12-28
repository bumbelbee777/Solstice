#include "CharacterPrimitives.hxx"
#include "GeometryOps.hxx"
#include "Polyhedra.hxx"
#include "Seed.hxx"
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <cmath>

namespace Solstice::Arzachel {

namespace Math = Solstice::Math;

Generator<MeshData> Head(const Seed& SeedParam, float Size) {
    return Generator<MeshData>([SeedParam, Size](const Seed& SeedParamInner) {
        // Basic head shape - slightly elongated sphere
        Generator<MeshData> HeadMesh = Sphere(0.12f * Size, 12, SeedParamInner);
        HeadMesh = Scale(HeadMesh, Math::Vec3(1, 1.2f, 1));
        HeadMesh = Transform(HeadMesh, Math::Matrix4::Translation(Math::Vec3(0, 0.15f * Size, 0)));

        // Simple facial features (early '00s style - minimal detail)
        // Eyes (simple indentations)
        Generator<MeshData> LeftEye = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(0.02f * Size, 0.01f * Size, 0.01f * Size));
        LeftEye = Transform(LeftEye, Math::Matrix4::Translation(Math::Vec3(-0.03f * Size, 0.12f * Size, 0.1f * Size)));

        Generator<MeshData> RightEye = Scale(Cube(1.0f, SeedParamInner.Derive(2)), Math::Vec3(0.02f * Size, 0.01f * Size, 0.01f * Size));
        RightEye = Transform(RightEye, Math::Matrix4::Translation(Math::Vec3(0.03f * Size, 0.12f * Size, 0.1f * Size)));

        Generator<MeshData> Result = Merge(HeadMesh, LeftEye);
        Result = Merge(Result, RightEye);

        return Result(SeedParamInner);
    });
}

Generator<MeshData> Torso(const Seed& SeedParam, int Build) {
    return Generator<MeshData>([SeedParam, Build](const Seed& SeedParamInner) {
        float Width = Build == 0 ? 0.3f : (Build == 1 ? 0.4f : 0.5f); // Slim, average, muscular
        float Depth = Build == 0 ? 0.2f : (Build == 1 ? 0.25f : 0.3f);
        float Height = 0.5f;

        Generator<MeshData> TorsoMesh = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Width, Height, Depth));
        TorsoMesh = Transform(TorsoMesh, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, 0)));

        return TorsoMesh(SeedParamInner);
    });
}

Generator<MeshData> Limb(const Seed& SeedParam, int Type, float Length) {
    return Generator<MeshData>([SeedParam, Type, Length](const Seed& SeedParamInner) {
        float Radius = Type == 0 ? 0.05f : 0.06f; // Arms thinner than legs

        Generator<MeshData> LimbMesh = Cylinder(Radius, Length, 12, SeedParamInner);
        LimbMesh = Transform(LimbMesh, Math::Matrix4::Translation(Math::Vec3(0, Length * 0.5f, 0)));

        return LimbMesh(SeedParamInner);
    });
}

Generator<MeshData> Hand(const Seed& SeedParam, float Size) {
    return Generator<MeshData>([SeedParam, Size](const Seed& SeedParamInner) {
        // Simple hand - box with slight finger separation
        Generator<MeshData> Palm = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.08f * Size, 0.03f * Size, 0.12f * Size));
        Palm = Transform(Palm, Math::Matrix4::Translation(Math::Vec3(0, 0.015f * Size, 0)));

        // Fingers (simplified)
        for (int i = 0; i < 4; ++i) {
            float x = (i - 1.5f) * 0.02f * Size;
            Generator<MeshData> Finger = Scale(Cube(1.0f, SeedParamInner.Derive(1 + i)), Math::Vec3(0.01f * Size, 0.05f * Size, 0.01f * Size));
            Finger = Transform(Finger, Math::Matrix4::Translation(Math::Vec3(x, 0.05f * Size, 0.06f * Size)));
            Palm = Merge(Palm, Finger);
        }

        return Palm(SeedParamInner);
    });
}

Generator<MeshData> Foot(const Seed& SeedParam, float Size) {
    return Generator<MeshData>([SeedParam, Size](const Seed& SeedParamInner) {
        // Simple foot/shoe
        Generator<MeshData> FootMesh = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.1f * Size, 0.05f * Size, 0.25f * Size));
        FootMesh = Transform(FootMesh, Math::Matrix4::Translation(Math::Vec3(0, 0.025f * Size, 0.05f * Size)));

        return FootMesh(SeedParamInner);
    });
}

Generator<MeshData> Hair(const Seed& SeedParam, int Style, float Length) {
    return Generator<MeshData>([SeedParam, Style, Length](const Seed& SeedParamInner) {
        // Simple hair mesh - varies by style
        if (Style == 0) { // Short
            Generator<MeshData> HairMesh = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.15f, Length * 0.3f, 0.15f));
            HairMesh = Transform(HairMesh, Math::Matrix4::Translation(Math::Vec3(0, 0.18f + Length * 0.15f, 0)));
            return HairMesh(SeedParamInner);
        } else if (Style == 1) { // Medium
            Generator<MeshData> HairMesh = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.15f, Length * 0.5f, 0.15f));
            HairMesh = Transform(HairMesh, Math::Matrix4::Translation(Math::Vec3(0, 0.18f + Length * 0.25f, 0)));
            return HairMesh(SeedParamInner);
        } else { // Long
            Generator<MeshData> HairMesh = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(0.15f, Length * 0.7f, 0.15f));
            HairMesh = Transform(HairMesh, Math::Matrix4::Translation(Math::Vec3(0, 0.18f + Length * 0.35f, 0)));
            return HairMesh(SeedParamInner);
        }
    });
}

Generator<MeshData> Clothing(const Seed& SeedParam, int Type, int Fit) {
    return Generator<MeshData>([SeedParam, Type, Fit](const Seed& SeedParamInner) {
        float FitFactor = Fit == 0 ? 1.0f : (Fit == 1 ? 1.1f : 1.2f); // Tight, normal, loose

        if (Type == 0) { // Shirt
            float Width = 0.35f * FitFactor;
            float Height = 0.5f;
            float Depth = 0.25f * FitFactor;

            Generator<MeshData> Shirt = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Width, Height, Depth));
            Shirt = Transform(Shirt, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, 0)));

            // Sleeves
            Generator<MeshData> LeftSleeve = Cylinder(0.06f * FitFactor, 0.3f, 12, SeedParamInner.Derive(1));
            LeftSleeve = Transform(LeftSleeve, Math::Matrix4::Translation(Math::Vec3(-Width * 0.6f, Height * 0.4f, 0)));

            Generator<MeshData> RightSleeve = Cylinder(0.06f * FitFactor, 0.3f, 12, SeedParamInner.Derive(2));
            RightSleeve = Transform(RightSleeve, Math::Matrix4::Translation(Math::Vec3(Width * 0.6f, Height * 0.4f, 0)));

            Generator<MeshData> Result = Merge(Shirt, LeftSleeve);
            Result = Merge(Result, RightSleeve);
            return Result(SeedParamInner);
        } else if (Type == 1) { // Pants
            float Width = 0.35f * FitFactor;
            float Height = 0.7f;
            float Depth = 0.25f * FitFactor;

            // Left leg
            Generator<MeshData> LeftLeg = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Width * 0.45f, Height, Depth * 0.45f));
            LeftLeg = Transform(LeftLeg, Math::Matrix4::Translation(Math::Vec3(-Width * 0.25f, Height * 0.5f, 0)));

            // Right leg
            Generator<MeshData> RightLeg = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(Width * 0.45f, Height, Depth * 0.45f));
            RightLeg = Transform(RightLeg, Math::Matrix4::Translation(Math::Vec3(Width * 0.25f, Height * 0.5f, 0)));

            Generator<MeshData> Result = Merge(LeftLeg, RightLeg);
            return Result(SeedParamInner);
        } else { // Jacket
            float Width = 0.4f * FitFactor;
            float Height = 0.55f;
            float Depth = 0.3f * FitFactor;

            Generator<MeshData> Jacket = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Width, Height, Depth));
            Jacket = Transform(Jacket, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, 0)));

            // Sleeves
            Generator<MeshData> LeftSleeve = Cylinder(0.07f * FitFactor, 0.35f, 12, SeedParamInner.Derive(1));
            LeftSleeve = Transform(LeftSleeve, Math::Matrix4::Translation(Math::Vec3(-Width * 0.6f, Height * 0.4f, 0)));

            Generator<MeshData> RightSleeve = Cylinder(0.07f * FitFactor, 0.35f, 12, SeedParamInner.Derive(2));
            RightSleeve = Transform(RightSleeve, Math::Matrix4::Translation(Math::Vec3(Width * 0.6f, Height * 0.4f, 0)));

            Generator<MeshData> Result = Merge(Jacket, LeftSleeve);
            Result = Merge(Result, RightSleeve);
            return Result(SeedParamInner);
        }
    });
}

Generator<MeshData> AssembleCharacter(
    const Generator<MeshData>& HeadGen,
    const Generator<MeshData>& TorsoGen,
    const Generator<MeshData>& ArmsGen,
    const Generator<MeshData>& LegsGen,
    const Generator<MeshData>& ClothingGen,
    const Seed& SeedParam
) {
    return Generator<MeshData>([HeadGen, TorsoGen, ArmsGen, LegsGen, ClothingGen, SeedParam](const Seed& SeedParamInner) {
        float TorsoHeight = 0.5f;
        float HeadHeight = 0.18f;
        float ArmLength = 0.4f;
        float LegLength = 0.7f;

        // Torso (base)
        Generator<MeshData> Torso = TorsoGen;
        Torso = Transform(Torso, Math::Matrix4::Translation(Math::Vec3(0, TorsoHeight * 0.5f, 0)));

        // Head
        Generator<MeshData> Head = HeadGen;
        Head = Transform(Head, Math::Matrix4::Translation(Math::Vec3(0, TorsoHeight + HeadHeight, 0)));

        // Left arm
        Generator<MeshData> LeftArm = ArmsGen;
        LeftArm = Transform(LeftArm, Math::Matrix4::Translation(Math::Vec3(-0.2f, TorsoHeight * 0.6f, 0)));
        LeftArm = Rotate(LeftArm, 0.3f, Math::Vec3(0, 0, 1));

        // Right arm
        Generator<MeshData> RightArm = ArmsGen;
        RightArm = Transform(RightArm, Math::Matrix4::Translation(Math::Vec3(0.2f, TorsoHeight * 0.6f, 0)));
        RightArm = Rotate(RightArm, -0.3f, Math::Vec3(0, 0, 1));

        // Left leg
        Generator<MeshData> LeftLeg = LegsGen;
        LeftLeg = Transform(LeftLeg, Math::Matrix4::Translation(Math::Vec3(-0.1f, LegLength * 0.5f, 0)));

        // Right leg
        Generator<MeshData> RightLeg = LegsGen;
        RightLeg = Transform(RightLeg, Math::Matrix4::Translation(Math::Vec3(0.1f, LegLength * 0.5f, 0)));

        // Clothing (overlay)
        Generator<MeshData> Clothing = ClothingGen;
        Clothing = Transform(Clothing, Math::Matrix4::Translation(Math::Vec3(0, TorsoHeight * 0.5f, 0)));

        // Assemble
        Generator<MeshData> Result = Merge(Torso, Head);
        Result = Merge(Result, LeftArm);
        Result = Merge(Result, RightArm);
        Result = Merge(Result, LeftLeg);
        Result = Merge(Result, RightLeg);
        Result = Merge(Result, Clothing);

        return Result(SeedParamInner);
    });
}

} // namespace Solstice::Arzachel

