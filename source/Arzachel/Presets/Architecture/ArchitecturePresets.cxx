#include "ArchitecturePresets.hxx"
#include "../../GeometryOps.hxx"
#include "../../Polyhedra.hxx"
#include "../../Seed.hxx"
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <cmath>

namespace Solstice::Arzachel {

namespace Math = Solstice::Math;

Generator<MeshData> Wall(const Seed& SeedParam, float Width, float Height) {
    return Generator<MeshData>([SeedParam, Width, Height](const Seed& SeedParamInner) {
        float Thickness = 0.2f;
        Generator<MeshData> WallMesh = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Width, Height, Thickness));
        WallMesh = Transform(WallMesh, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, 0)));
        return WallMesh(SeedParamInner);
    });
}

Generator<MeshData> Door(const Seed& SeedParam, float Width, float Height, int Type) {
    return Generator<MeshData>([SeedParam, Width, Height, Type](const Seed& SeedParamInner) {
        float Thickness = 0.05f;
        
        if (Type == 1) { // Double door
            // Left panel
            Generator<MeshData> LeftPanel = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Width * 0.48f, Height, Thickness));
            LeftPanel = Transform(LeftPanel, Math::Matrix4::Translation(Math::Vec3(-Width * 0.24f, Height * 0.5f, 0)));

            // Right panel
            Generator<MeshData> RightPanel = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(Width * 0.48f, Height, Thickness));
            RightPanel = Transform(RightPanel, Math::Matrix4::Translation(Math::Vec3(Width * 0.24f, Height * 0.5f, 0)));

            Generator<MeshData> Result = Merge(LeftPanel, RightPanel);
            return Result(SeedParamInner);
        } else if (Type == 2) { // Sliding door
            // Door panel
            Generator<MeshData> Panel = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Width, Height, Thickness));
            Panel = Transform(Panel, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, 0)));

            // Track
            Generator<MeshData> Track = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(Width * 1.2f, 0.05f, 0.1f));
            Track = Transform(Track, Math::Matrix4::Translation(Math::Vec3(0, Height + 0.025f, 0)));

            Generator<MeshData> Result = Merge(Panel, Track);
            return Result(SeedParamInner);
        } else if (Type == 3) { // Security door
            // Door with reinforcement
            Generator<MeshData> Panel = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Width, Height, Thickness * 1.5f));
            Panel = Transform(Panel, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, 0)));

            // Reinforcement bars
            for (int i = 0; i < 3; ++i) {
                float y = Height * (0.25f + i * 0.25f);
                Generator<MeshData> Bar = Scale(Cube(1.0f, SeedParamInner.Derive(1 + i)), Math::Vec3(Width * 0.9f, 0.05f, 0.05f));
                Bar = Transform(Bar, Math::Matrix4::Translation(Math::Vec3(0, y, Thickness * 0.8f)));
                Panel = Merge(Panel, Bar);
            }

            return Panel(SeedParamInner);
        } else { // Single door
            Generator<MeshData> Panel = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Width, Height, Thickness));
            Panel = Transform(Panel, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, 0)));
            return Panel(SeedParamInner);
        }
    });
}

Generator<MeshData> Window(const Seed& SeedParam, float Width, float Height, int Type) {
    return Generator<MeshData>([SeedParam, Width, Height, Type](const Seed& SeedParamInner) {
        float FrameThickness = 0.05f;
        
        // Frame
        Generator<MeshData> Frame = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Width + FrameThickness * 2, Height + FrameThickness * 2, 0.1f));
        Frame = Transform(Frame, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, 0)));

        // Glass (hollow center)
        Generator<MeshData> Glass = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(Width, Height, 0.01f));
        Glass = Transform(Glass, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, 0.05f)));

        Generator<MeshData> Result = Merge(Frame, Glass);

        if (Type == 1) { // Reinforced - add bars
            for (int i = 0; i < 3; ++i) {
                float y = Height * (0.25f + i * 0.25f);
                Generator<MeshData> Bar = Scale(Cube(1.0f, SeedParamInner.Derive(10 + i)), Math::Vec3(Width, 0.03f, 0.03f));
                Bar = Transform(Bar, Math::Matrix4::Translation(Math::Vec3(0, y, 0.05f)));
                Result = Merge(Result, Bar);
            }
        } else if (Type == 2) { // Barred - add vertical bars
            int BarCount = 5;
            for (int i = 0; i < BarCount; ++i) {
                float x = (i / static_cast<float>(BarCount - 1)) * Width - Width * 0.5f;
                Generator<MeshData> Bar = Scale(Cube(1.0f, SeedParamInner.Derive(20 + i)), Math::Vec3(0.03f, Height, 0.03f));
                Bar = Transform(Bar, Math::Matrix4::Translation(Math::Vec3(x, Height * 0.5f, 0.05f)));
                Result = Merge(Result, Bar);
            }
        }

        return Result(SeedParamInner);
    });
}

Generator<MeshData> Stairs(const Seed& SeedParam, int Steps, float Width, int Type) {
    return Generator<MeshData>([SeedParam, Steps, Width, Type](const Seed& SeedParamInner) {
        float StepHeight = 0.15f;
        float StepDepth = 0.3f;
        float TotalHeight = Steps * StepHeight;
        float TotalDepth = Steps * StepDepth;

        Generator<MeshData> Result = Generator<MeshData>([](const Seed&) { return MeshData(); });

        if (Type == 1) { // Spiral
            float Radius = Width * 0.5f;
            for (int i = 0; i < Steps; ++i) {
                float angle = (i / static_cast<float>(Steps)) * 2.0f * 3.14159f;
                float y = i * StepHeight;
                float x = std::cos(angle) * Radius;
                float z = std::sin(angle) * Radius;
                
                Generator<MeshData> Step = Scale(Cube(1.0f, SeedParamInner.Derive(i)), Math::Vec3(Width * 0.6f, StepHeight, StepDepth));
                Step = Transform(Step, Math::Matrix4::Translation(Math::Vec3(x, y + StepHeight * 0.5f, z)));
                
                if (i == 0) {
                    Result = Step;
                } else {
                    Result = Merge(Result, Step);
                }
            }
        } else if (Type == 2) { // L-shaped
            int MidStep = Steps / 2;
            for (int i = 0; i < Steps; ++i) {
                float y = i * StepHeight;
                float x = i < MidStep ? 0 : (i - MidStep) * StepDepth;
                float z = i < MidStep ? i * StepDepth : MidStep * StepDepth;
                
                Generator<MeshData> Step = Scale(Cube(1.0f, SeedParamInner.Derive(i)), Math::Vec3(Width, StepHeight, StepDepth));
                Step = Transform(Step, Math::Matrix4::Translation(Math::Vec3(x, y + StepHeight * 0.5f, z)));
                
                if (i == 0) {
                    Result = Step;
                } else {
                    Result = Merge(Result, Step);
                }
            }
        } else if (Type == 3) { // U-shaped
            int MidStep = Steps / 3;
            for (int i = 0; i < Steps; ++i) {
                float y = i * StepHeight;
                float x = i < MidStep ? 0 : (i < Steps - MidStep ? MidStep * StepDepth : (Steps - i) * StepDepth);
                float z = i < MidStep ? i * StepDepth : (i < Steps - MidStep ? MidStep * StepDepth : (Steps - i) * StepDepth);
                
                Generator<MeshData> Step = Scale(Cube(1.0f, SeedParamInner.Derive(i)), Math::Vec3(Width, StepHeight, StepDepth));
                Step = Transform(Step, Math::Matrix4::Translation(Math::Vec3(x, y + StepHeight * 0.5f, z)));
                
                if (i == 0) {
                    Result = Step;
                } else {
                    Result = Merge(Result, Step);
                }
            }
        } else { // Straight
            for (int i = 0; i < Steps; ++i) {
                float y = i * StepHeight;
                float z = i * StepDepth;
                
                Generator<MeshData> Step = Scale(Cube(1.0f, SeedParamInner.Derive(i)), Math::Vec3(Width, StepHeight, StepDepth));
                Step = Transform(Step, Math::Matrix4::Translation(Math::Vec3(0, y + StepHeight * 0.5f, z)));
                
                if (i == 0) {
                    Result = Step;
                } else {
                    Result = Merge(Result, Step);
                }
            }
        }

        return Result(SeedParamInner);
    });
}

Generator<MeshData> Railing(const Seed& SeedParam, float Length, int Type) {
    return Generator<MeshData>([SeedParam, Length, Type](const Seed& SeedParamInner) {
        float Height = 0.9f;
        float PostSpacing = 1.0f;
        int PostCount = static_cast<int>(Length / PostSpacing) + 1;

        // Top rail
        Generator<MeshData> TopRail = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Length, 0.05f, 0.05f));
        TopRail = Transform(TopRail, Math::Matrix4::Translation(Math::Vec3(0, Height, 0)));

        Generator<MeshData> Result = TopRail;

        // Posts
        for (int i = 0; i < PostCount; ++i) {
            float x = (i / static_cast<float>(PostCount - 1)) * Length - Length * 0.5f;
            Generator<MeshData> Post = Scale(Cube(1.0f, SeedParamInner.Derive(1 + i)), Math::Vec3(0.05f, Height, 0.05f));
            Post = Transform(Post, Math::Matrix4::Translation(Math::Vec3(x, Height * 0.5f, 0)));
            Result = Merge(Result, Post);
        }

        // Bottom rail (for industrial/decorative)
        if (Type >= 1) {
            Generator<MeshData> BottomRail = Scale(Cube(1.0f, SeedParamInner.Derive(100)), Math::Vec3(Length, 0.05f, 0.05f));
            BottomRail = Transform(BottomRail, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.3f, 0)));
            Result = Merge(Result, BottomRail);
        }

        return Result(SeedParamInner);
    });
}

Generator<MeshData> Hallway(const Seed& SeedParam, float Length, float Width, float Height) {
    return Generator<MeshData>([SeedParam, Length, Width, Height](const Seed& SeedParamInner) {
        float WallThickness = 0.2f;

        // Floor
        Generator<MeshData> Floor = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Length, 0.1f, Width));
        Floor = Transform(Floor, Math::Matrix4::Translation(Math::Vec3(0, 0.05f, 0)));

        // Ceiling
        Generator<MeshData> Ceiling = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(Length, 0.1f, Width));
        Ceiling = Transform(Ceiling, Math::Matrix4::Translation(Math::Vec3(0, Height - 0.05f, 0)));

        // Left wall
        Generator<MeshData> LeftWall = Scale(Cube(1.0f, SeedParamInner.Derive(2)), Math::Vec3(Length, Height, WallThickness));
        LeftWall = Transform(LeftWall, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, -Width * 0.5f - WallThickness * 0.5f)));

        // Right wall
        Generator<MeshData> RightWall = Scale(Cube(1.0f, SeedParamInner.Derive(3)), Math::Vec3(Length, Height, WallThickness));
        RightWall = Transform(RightWall, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, Width * 0.5f + WallThickness * 0.5f)));

        Generator<MeshData> Result = Merge(Floor, Ceiling);
        Result = Merge(Result, LeftWall);
        Result = Merge(Result, RightWall);

        return Result(SeedParamInner);
    });
}

Generator<MeshData> Corridor(const Seed& SeedParam, float Length, float Width, float Height) {
    // Similar to hallway but may have additional features
    return Hallway(SeedParam, Length, Width, Height);
}

Generator<MeshData> Room(const Seed& SeedParam, float Width, float Depth, float Height, int Type) {
    return Generator<MeshData>([SeedParam, Width, Depth, Height, Type](const Seed& SeedParamInner) {
        float WallThickness = 0.2f;

        // Floor
        Generator<MeshData> Floor = Scale(Cube(1.0f, SeedParamInner), Math::Vec3(Width, 0.1f, Depth));
        Floor = Transform(Floor, Math::Matrix4::Translation(Math::Vec3(0, 0.05f, 0)));

        // Ceiling
        Generator<MeshData> Ceiling = Scale(Cube(1.0f, SeedParamInner.Derive(1)), Math::Vec3(Width, 0.1f, Depth));
        Ceiling = Transform(Ceiling, Math::Matrix4::Translation(Math::Vec3(0, Height - 0.05f, 0)));

        // Walls (4 walls)
        Generator<MeshData> FrontWall = Scale(Cube(1.0f, SeedParamInner.Derive(2)), Math::Vec3(Width, Height, WallThickness));
        FrontWall = Transform(FrontWall, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, Depth * 0.5f + WallThickness * 0.5f)));

        Generator<MeshData> BackWall = Scale(Cube(1.0f, SeedParamInner.Derive(3)), Math::Vec3(Width, Height, WallThickness));
        BackWall = Transform(BackWall, Math::Matrix4::Translation(Math::Vec3(0, Height * 0.5f, -Depth * 0.5f - WallThickness * 0.5f)));

        Generator<MeshData> LeftWall = Scale(Cube(1.0f, SeedParamInner.Derive(4)), Math::Vec3(WallThickness, Height, Depth));
        LeftWall = Transform(LeftWall, Math::Matrix4::Translation(Math::Vec3(-Width * 0.5f - WallThickness * 0.5f, Height * 0.5f, 0)));

        Generator<MeshData> RightWall = Scale(Cube(1.0f, SeedParamInner.Derive(5)), Math::Vec3(WallThickness, Height, Depth));
        RightWall = Transform(RightWall, Math::Matrix4::Translation(Math::Vec3(Width * 0.5f + WallThickness * 0.5f, Height * 0.5f, 0)));

        Generator<MeshData> Result = Merge(Floor, Ceiling);
        Result = Merge(Result, FrontWall);
        Result = Merge(Result, BackWall);
        Result = Merge(Result, LeftWall);
        Result = Merge(Result, RightWall);

        return Result(SeedParamInner);
    });
}

} // namespace Solstice::Arzachel

