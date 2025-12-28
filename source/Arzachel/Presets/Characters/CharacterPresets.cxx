#include "CharacterPresets.hxx"
#include "../../CharacterPrimitives.hxx"
#include "../Accessories/AccessoryPresets.hxx"
#include "../../GeometryOps.hxx"
#include "../../Seed.hxx"
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>

namespace Solstice::Arzachel {

namespace Math = Solstice::Math;

Generator<MeshData> Scientist(const Seed& SeedParam, int Gender) {
    return Generator<MeshData>([SeedParam, Gender](const Seed& SeedParamInner) {
        // Scientist character - lab coat, glasses, etc.
        int Build = Gender == 0 ? 0 : 0; // Slim build for both
        
        // Head
        Generator<MeshData> HeadMesh = Head(SeedParamInner.Derive(0), 1.0f);
        
        // Torso (slim)
        Generator<MeshData> TorsoMesh = Torso(SeedParamInner.Derive(1), Build);
        
        // Arms
        Generator<MeshData> ArmsMesh = Limb(SeedParamInner.Derive(2), 0, 0.4f);
        
        // Legs
        Generator<MeshData> LegsMesh = Limb(SeedParamInner.Derive(3), 1, 0.7f);
        
        // Lab coat (jacket type, loose fit)
        Generator<MeshData> LabCoat = Clothing(SeedParamInner.Derive(4), 2, 2); // Jacket, loose
        
        // Assemble
        Generator<MeshData> Character = AssembleCharacter(
            HeadMesh,
            TorsoMesh,
            ArmsMesh,
            LegsMesh,
            LabCoat,
            SeedParamInner.Derive(5)
        );

        return Character(SeedParamInner);
    });
}

Generator<MeshData> Soldier(const Seed& SeedParam, int Type) {
    return Generator<MeshData>([SeedParam, Type](const Seed& SeedParamInner) {
        // Soldier character - uniform, helmet
        int Build = Type == 0 ? 1 : 0; // Standard = average, Officer = slim
        
        // Head
        Generator<MeshData> HeadMesh = Head(SeedParamInner.Derive(0), 1.0f);
        
        // Torso
        Generator<MeshData> TorsoMesh = Torso(SeedParamInner.Derive(1), Build);
        
        // Arms
        Generator<MeshData> ArmsMesh = Limb(SeedParamInner.Derive(2), 0, 0.4f);
        
        // Legs
        Generator<MeshData> LegsMesh = Limb(SeedParamInner.Derive(3), 1, 0.7f);
        
        // Uniform (pants + shirt, normal fit)
        Generator<MeshData> Shirt = Clothing(SeedParamInner.Derive(4), 0, 1); // Shirt, normal
        Generator<MeshData> Pants = Clothing(SeedParamInner.Derive(5), 1, 1); // Pants, normal
        
        // Combine clothing
        Generator<MeshData> Uniform = Merge(Shirt, Pants);
        
        // Helmet (combat type)
        Generator<MeshData> HelmetMesh = Helmet(SeedParamInner.Derive(6), 1); // Combat helmet
        HelmetMesh = Transform(HelmetMesh, Math::Matrix4::Translation(Math::Vec3(0, 0.18f, 0)));
        HeadMesh = Merge(HeadMesh, HelmetMesh);
        
        // Assemble
        Generator<MeshData> Character = AssembleCharacter(
            HeadMesh,
            TorsoMesh,
            ArmsMesh,
            LegsMesh,
            Uniform,
            SeedParamInner.Derive(7)
        );

        return Character(SeedParamInner);
    });
}

Generator<MeshData> JohnLeeman(const Seed& SeedParam) {
    return Generator<MeshData>([SeedParam](const Seed& SeedParamInner) {
        // John Leeman: American systems engineer, early 30s
        // Slim and agile, muted green eyes, medium-length blond hair, light stubble
        // Casual professional attire: oxford shirts, sweaters, unstructured jacket, straight trousers/dark jeans, worn leather shoes
        
        // Head (slim, early 30s)
        Generator<MeshData> HeadMesh = Head(SeedParamInner.Derive(0), 1.0f);
        
        // Medium-length blond hair (style 1 = medium, length ~0.15)
        Generator<MeshData> HairMesh = Hair(SeedParamInner.Derive(1), 1, 0.15f);
        HairMesh = Transform(HairMesh, Math::Matrix4::Translation(Math::Vec3(0, 0.18f, 0)));
        HeadMesh = Merge(HeadMesh, HairMesh);
        
        // Torso (slim build - build 0)
        Generator<MeshData> TorsoMesh = Torso(SeedParamInner.Derive(2), 0);
        
        // Arms (slim)
        Generator<MeshData> ArmsMesh = Limb(SeedParamInner.Derive(3), 0, 0.4f);
        
        // Legs (slim)
        Generator<MeshData> LegsMesh = Limb(SeedParamInner.Derive(4), 1, 0.7f);
        
        // Casual professional clothing
        // Unstructured jacket (jacket type, normal fit)
        Generator<MeshData> Jacket = Clothing(SeedParamInner.Derive(5), 2, 1);
        
        // Straight trousers (pants, normal fit)
        Generator<MeshData> Trousers = Clothing(SeedParamInner.Derive(6), 1, 1);
        
        // Combine clothing
        Generator<MeshData> Outfit = Merge(Jacket, Trousers);
        
        // Assemble John Leeman
        Generator<MeshData> Character = AssembleCharacter(
            HeadMesh,
            TorsoMesh,
            ArmsMesh,
            LegsMesh,
            Outfit,
            SeedParamInner.Derive(7)
        );

        return Character(SeedParamInner);
    });
}

} // namespace Solstice::Arzachel

