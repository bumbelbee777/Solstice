#include "AssetPresets.hxx"
#include "GeometryOps.hxx"
#include "Polyhedra.hxx"
#include "Seed.hxx"
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <cmath>

namespace Solstice::Arzachel {

namespace Math = Solstice::Math;

Generator<MeshData> Building(const Seed& SeedParam, int Floors) {
    return Generator<MeshData>([SeedParam, Floors](const Seed& SeedParamInner) {
        Generator<MeshData> Result = Cube(1.0f, SeedParamInner);
        for (int I = 1; I < Floors; ++I) {
            Result = Merge(Result, Transform(Cube(1.0f, SeedParamInner.Derive(I)), Math::Matrix4::Translation(Math::Vec3(0, (float)I, 0))));
        }
        return Result(SeedParamInner);
    });
}

Generator<MeshData> Car(const Seed& SeedParam) {
    return Generator<MeshData>([](const Seed& SeedParamInner) {
        Generator<MeshData> Body = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(2.0f, 0.5f, 1.0f));
        Generator<MeshData> Cabin = Transform(Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(1.0f, 0.5f, 0.8f)), Math::Matrix4::Translation(Math::Vec3(0, 0.5f, 0)));
        return Merge(Body, Cabin)(SeedParamInner);
    });
}

Generator<MeshData> Ship(const Seed& SeedParam) {
    return Generator<MeshData>([](const Seed& SeedParamInner) {
        return Scale(Cube(1.0f, SeedParamInner), Math::Vec3(4.0f, 0.5f, 1.5f))(SeedParamInner);
    });
}

Generator<MeshData> PlaneAsset(const Seed& SeedParam) {
    return Generator<MeshData>([](const Seed& SeedParamInner) {
        Generator<MeshData> Fuselage = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(3.0f, 0.4f, 0.4f));
        Generator<MeshData> Wings = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(0.5f, 0.05f, 4.0f));
        return Merge(Fuselage, Wings)(SeedParamInner);
    });
}

Generator<MeshData> Shack(const Seed& SeedParam) {
    return Generator<MeshData>([](const Seed& SeedParamInner) {
        Generator<MeshData> Base = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(2.0f, 1.5f, 2.0f));
        Generator<MeshData> Roof = Transform(Scale(Cube(1.1f, SeedParamInner.Derive(1)), Math::Vec3(2.2f, 0.2f, 2.2f)), Math::Matrix4::Translation(Math::Vec3(0, 0.85f, 0)));
        return Merge(Base, Roof)(SeedParamInner);
    });
}

Generator<MeshData> Cabin(const Seed& SeedParam) {
    return Generator<MeshData>([](const Seed& SeedParamInner) {
        Generator<MeshData> MainBody = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(4.0f, 2.5f, 3.0f));
        Generator<MeshData> Porch = Transform(Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(4.0f, 0.2f, 1.0f)), Math::Matrix4::Translation(Math::Vec3(0, -1.15f, 2.0f)));
        Generator<MeshData> Roof = Transform(Scale(Cube(1.0f, SeedParamInner.Derive(2)), Math::Vec3(4.5f, 0.5f, 4.0f)), Math::Matrix4::Translation(Math::Vec3(0, 1.5f, 0.5f)));
        return Merge(Merge(MainBody, Porch), Roof)(SeedParamInner);
    });
}

Generator<MeshData> Damaged(const Generator<MeshData>& Mesh, const Seed& SeedParam, float Amount) {
    return Generator<MeshData>([Mesh, SeedParam, Amount](const Seed& SeedParamInner) {
        MeshData Result = Mesh(SeedParamInner);
        for (size_t I = 0; I < Result.Positions.size(); ++I) {
            Seed S = SeedParamInner.Derive(static_cast<uint32_t>(I));
            Result.Positions[I] += Math::Vec3(NextFloat(S) - 0.5f, NextFloat(S.Derive(1)) - 0.5f, NextFloat(S.Derive(2)) - 0.5f) * Amount;
        }
        Result.CalculateBounds();
        return Result;
    });
}

Generator<MeshData> PineTree(const Seed& SeedParam, float Height, bool SnowCovered) {
    return Generator<MeshData>([SeedParam, Height, SnowCovered](const Seed& SeedParamInner) {
        // Trunk (cylinder)
        float TrunkHeight = Height * 0.3f;
        float TrunkRadius = Height * 0.05f;
        Generator<MeshData> Trunk = Scale(Cylinder(TrunkRadius, TrunkHeight, 8, SeedParamInner), Math::Vec3(1, 1, 1));
        Trunk = Transform(Trunk, Math::Matrix4::Translation(Math::Vec3(0, TrunkHeight * 0.5f, 0)));

        // Branches (cones) - multiple layers
        Generator<MeshData> Branches = Generator<MeshData>([](const Seed&) { return MeshData(); });
        int LayerCount = 4;
        for (int i = 0; i < LayerCount; ++i) {
            float LayerY = TrunkHeight + (Height - TrunkHeight) * (static_cast<float>(i) / static_cast<float>(LayerCount));
            float LayerHeight = (Height - TrunkHeight) / static_cast<float>(LayerCount);
            float LayerRadius = Height * 0.15f * (1.0f - static_cast<float>(i) * 0.2f);

            // Create cone from cylinder scaled to point
            Generator<MeshData> Layer = Cylinder(LayerRadius, LayerHeight, 8, SeedParamInner.Derive(i));
            Layer = Scale(Layer, Math::Vec3(1, 1, 1));
            Layer = Transform(Layer, Math::Matrix4::Translation(Math::Vec3(0, LayerY + LayerHeight * 0.5f, 0)));

            if (i == 0) {
                Branches = Layer;
            } else {
                Branches = Merge(Branches, Layer);
            }
        }

        Generator<MeshData> Tree = Merge(Trunk, Branches);

        // Add snow layer on top if snow-covered
        if (SnowCovered) {
            float SnowHeight = Height * 0.1f;
            float SnowRadius = Height * 0.12f;
            Generator<MeshData> Snow = Scale(Cylinder(SnowRadius, SnowHeight, 8, SeedParamInner.Derive(100)), Math::Vec3(1, 1, 1));
            Snow = Transform(Snow, Math::Matrix4::Translation(Math::Vec3(0, Height - SnowHeight * 0.5f, 0)));
            Tree = Merge(Tree, Snow);
        }

        return Tree(SeedParamInner);
    });
}

Generator<MeshData> Road(const Seed& SeedParam, float Length, float Width) {
    return Generator<MeshData>([SeedParam, Length, Width](const Seed& SeedParamInner) {
        // Create flat road mesh with slight beveled edges
        float HalfLength = Length * 0.5f;
        float HalfWidth = Width * 0.5f;
        float EdgeBevel = Width * 0.05f;

        // Main road surface
        Generator<MeshData> RoadSurface = Scale(Plane(Length, Width, SeedParamInner), Math::Vec3(1, 1, 1));
        RoadSurface = Transform(RoadSurface, Math::Matrix4::Translation(Math::Vec3(0, 0.01f, 0)));

        // Road markings (center line and edges) - simplified as submeshes would be better
        // For now, just return the road surface
        return RoadSurface(SeedParamInner);
    });
}

Generator<MeshData> SwissHouse(const Seed& SeedParam, int Floors) {
    return Generator<MeshData>([SeedParam, Floors](const Seed& SeedParamInner) {
        float FloorHeight = 2.5f;
        float BaseWidth = 6.0f;
        float BaseDepth = 5.0f;

        // Main building body
        Generator<MeshData> Building = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(BaseWidth, FloorHeight * Floors, BaseDepth));
        Building = Transform(Building, Math::Matrix4::Translation(Math::Vec3(0, FloorHeight * Floors * 0.5f, 0)));

        // Steep pitched roof
        float RoofHeight = BaseWidth * 0.4f;
        Generator<MeshData> Roof = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(BaseWidth * 1.1f, RoofHeight, BaseDepth * 1.1f));
        Roof = Transform(Roof, Math::Matrix4::Translation(Math::Vec3(0, FloorHeight * Floors + RoofHeight * 0.5f, 0)));

        // Balcony (if multiple floors)
        Generator<MeshData> Result = Merge(Building, Roof);
        if (Floors > 1) {
            float BalconyY = FloorHeight * (Floors - 1) + 0.5f;
            Generator<MeshData> Balcony = Scale(Cube(1.0f, SeedParamInner.Derive(2)), Math::Vec3(BaseWidth * 1.2f, 0.2f, 1.5f));
            Balcony = Transform(Balcony, Math::Matrix4::Translation(Math::Vec3(0, BalconyY, BaseDepth * 0.4f)));
            Result = Merge(Result, Balcony);
        }

        return Result(SeedParamInner);
    });
}

Generator<MeshData> SwissChurch(const Seed& SeedParam) {
    return Generator<MeshData>([SeedParam](const Seed& SeedParamInner) {
        float BaseWidth = 8.0f;
        float BaseDepth = 10.0f;
        float BaseHeight = 8.0f;

        // Main church body
        Generator<MeshData> Body = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(BaseWidth, BaseHeight, BaseDepth));
        Body = Transform(Body, Math::Matrix4::Translation(Math::Vec3(0, BaseHeight * 0.5f, 0)));

        // Spire/tower
        float SpireHeight = BaseHeight * 1.5f;
        float SpireWidth = BaseWidth * 0.3f;
        Generator<MeshData> Spire = Scale(Cylinder(SpireWidth, SpireHeight, 8, SeedParamInner.Derive(1)), Math::Vec3(1, 1, 1));
        Spire = Transform(Spire, Math::Matrix4::Translation(Math::Vec3(0, BaseHeight + SpireHeight * 0.5f, 0)));

        // Steep roof
        float RoofHeight = BaseWidth * 0.4f;
        Generator<MeshData> Roof = Scale(Cube(1.0f, SeedParamInner.Derive(2)), Math::Vec3(BaseWidth * 1.1f, RoofHeight, BaseDepth * 1.1f));
        Roof = Transform(Roof, Math::Matrix4::Translation(Math::Vec3(0, BaseHeight + RoofHeight * 0.5f, 0)));

        return Merge(Merge(Body, Spire), Roof)(SeedParamInner);
    });
}

Generator<MeshData> SwissTownHall(const Seed& SeedParam) {
    return Generator<MeshData>([SeedParam](const Seed& SeedParamInner) {
        float BaseWidth = 12.0f;
        float BaseDepth = 10.0f;
        float BaseHeight = 6.0f;

        // Main building
        Generator<MeshData> Building = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(BaseWidth, BaseHeight, BaseDepth));
        Building = Transform(Building, Math::Matrix4::Translation(Math::Vec3(0, BaseHeight * 0.5f, 0)));

        // Columns/pillars
        Generator<MeshData> Columns = Generator<MeshData>([](const Seed&) { return MeshData(); });
        for (int i = 0; i < 4; ++i) {
            float ColX = (i % 2 == 0 ? -1 : 1) * BaseWidth * 0.35f;
            float ColZ = (i < 2 ? -1 : 1) * BaseDepth * 0.35f;
            Generator<MeshData> Col = Scale(Cylinder(0.3f, BaseHeight * 0.8f, 8, SeedParamInner.Derive(i + 10)), Math::Vec3(1, 1, 1));
            Col = Transform(Col, Math::Matrix4::Translation(Math::Vec3(ColX, BaseHeight * 0.4f, ColZ)));
            if (i == 0) {
                Columns = Col;
            } else {
                Columns = Merge(Columns, Col);
            }
        }

        // Roof
        float RoofHeight = BaseWidth * 0.3f;
        Generator<MeshData> Roof = Scale(Cube(1.0f, SeedParamInner.Derive(20)), Math::Vec3(BaseWidth * 1.1f, RoofHeight, BaseDepth * 1.1f));
        Roof = Transform(Roof, Math::Matrix4::Translation(Math::Vec3(0, BaseHeight + RoofHeight * 0.5f, 0)));

        return Merge(Merge(Building, Columns), Roof)(SeedParamInner);
    });
}

Generator<MeshData> SwissShop(const Seed& SeedParam) {
    return Generator<MeshData>([SeedParam](const Seed& SeedParamInner) {
        float BaseWidth = 5.0f;
        float BaseDepth = 4.0f;
        float BaseHeight = 3.5f;

        // Main shop body
        Generator<MeshData> Shop = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(BaseWidth, BaseHeight, BaseDepth));
        Shop = Transform(Shop, Math::Matrix4::Translation(Math::Vec3(0, BaseHeight * 0.5f, 0)));

        // Shop window (larger opening)
        Generator<MeshData> Window = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(BaseWidth * 0.6f, BaseHeight * 0.4f, 0.1f));
        Window = Transform(Window, Math::Matrix4::Translation(Math::Vec3(0, BaseHeight * 0.3f, BaseDepth * 0.51f)));

        // Roof
        float RoofHeight = BaseWidth * 0.35f;
        Generator<MeshData> Roof = Scale(Cube(1.0f, SeedParamInner.Derive(2)), Math::Vec3(BaseWidth * 1.1f, RoofHeight, BaseDepth * 1.1f));
        Roof = Transform(Roof, Math::Matrix4::Translation(Math::Vec3(0, BaseHeight + RoofHeight * 0.5f, 0)));

        return Merge(Merge(Shop, Window), Roof)(SeedParamInner);
    });
}

Generator<MeshData> GrassPatch(const Seed& SeedParam, float Size) {
    return Generator<MeshData>([SeedParam, Size](const Seed& SeedParamInner) {
        // Simple grass patch as a low, slightly irregular plane
        float PatchSize = Size;
        Generator<MeshData> Patch = Scale(Plane(PatchSize, PatchSize, SeedParamInner), Math::Vec3(1, 1, 1));
        // Add slight height variation for realism
        MeshData Result = Patch(SeedParamInner);
        for (size_t i = 0; i < Result.Positions.size(); ++i) {
            Seed S = SeedParamInner.Derive(static_cast<uint32_t>(i));
            Result.Positions[i].y += (NextFloat(S) - 0.5f) * Size * 0.1f;
        }
        Result.CalculateBounds();
        return Result;
    });
}

Generator<MeshData> SmallBush(const Seed& SeedParam, float Height) {
    return Generator<MeshData>([SeedParam, Height](const Seed& SeedParamInner) {
        // Bush as a low, wide sphere
        float BushRadius = Height * 0.8f;
        Generator<MeshData> Bush = Scale(Sphere(BushRadius, 8, SeedParamInner), Math::Vec3(1, 0.6f, 1)); // Flatten vertically
        Bush = Transform(Bush, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.3f, 0)));
        return Bush(SeedParamInner);
    });
}

Generator<MeshData> Rock(const Seed& SeedParam, float Size) {
    return Generator<MeshData>([SeedParam, Size](const Seed& SeedParamInner) {
        // Rock as an irregular sphere/cube hybrid
        Generator<MeshData> BaseRock = Scale(Cube(Size, SeedParamInner), Math::Vec3(1, 1, 1));
        MeshData Result = BaseRock(SeedParamInner);
        // Add random variation to make it look more rock-like
        for (size_t i = 0; i < Result.Positions.size(); ++i) {
            Seed S = SeedParamInner.Derive(static_cast<uint32_t>(i));
            float Variation = (NextFloat(S) - 0.5f) * Size * 0.3f;
            Result.Positions[i] += Result.Normals[i] * Variation;
        }
        Result.CalculateBounds();
        return Result;
    });
}

Generator<MeshData> SnowDrift(const Seed& SeedParam, float Width, float Height) {
    return Generator<MeshData>([SeedParam, Width, Height](const Seed& SeedParamInner) {
        // Snow drift as a smooth, elongated mound
        float HalfWidth = Width * 0.5f;
        Generator<MeshData> Drift = Scale(Sphere(HalfWidth, 12, SeedParamInner), Math::Vec3(1, Height / HalfWidth, 1));
        Drift = Transform(Drift, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, 0)));
        return Drift(SeedParamInner);
    });
}

} // namespace Solstice::Arzachel
