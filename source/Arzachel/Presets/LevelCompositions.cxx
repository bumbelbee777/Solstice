#include "LevelCompositions.hxx"
#include "Architecture/ArchitecturePresets.hxx"
#include "Furniture/FurniturePresets.hxx"
#include "Industrial/IndustrialPresets.hxx"
#include "../GeometryOps.hxx"
#include "../Polyhedra.hxx"
#include "../Seed.hxx"
#include "../Generator.hxx"
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <Math/Quaternion.hxx>
#include <cmath>
#include <algorithm>

namespace Solstice::Arzachel {

namespace Math = Solstice::Math;

Generator<MeshData> OfficeRoom(const Seed& SeedParam, float Size) {
    return Generator<MeshData>([SeedParam, Size](const Seed& SeedParamInner) {
        float Width = 4.0f * Size;
        float Depth = 3.0f * Size;
        float Height = 2.5f;

        // Room structure
        Generator<MeshData> RoomMesh = Room(SeedParamInner.Derive(0), Width, Depth, Height, 0);

        // Desk
        Generator<MeshData> DeskMesh = Desk(SeedParamInner.Derive(10), 1.2f, 0.6f);
        DeskMesh = Transform(DeskMesh, Math::Matrix4::Translation(Math::Vec3(-Width * 0.3f, 0, -Depth * 0.3f)));

        // Chair
        Generator<MeshData> ChairMesh = Chair(SeedParamInner.Derive(11), 0);
        ChairMesh = Transform(ChairMesh, Math::Matrix4::Translation(Math::Vec3(-Width * 0.3f, 0, -Depth * 0.1f)));

        // Computer setup
        Generator<MeshData> ComputerMesh = Computer(SeedParamInner.Derive(12), 0);
        ComputerMesh = Transform(ComputerMesh, Math::Matrix4::Translation(Math::Vec3(-Width * 0.3f, 0.75f, -Depth * 0.3f)));

        Generator<MeshData> MonitorMesh = Monitor(SeedParamInner.Derive(13), 1.0f, 0);
        MonitorMesh = Transform(MonitorMesh, Math::Matrix4::Translation(Math::Vec3(-Width * 0.3f, 0.75f, -Depth * 0.25f)));

        // Filing cabinet
        Generator<MeshData> FilingCabinetMesh = FilingCabinet(SeedParamInner.Derive(14));
        FilingCabinetMesh = Transform(FilingCabinetMesh, Math::Matrix4::Translation(Math::Vec3(Width * 0.35f, 0, -Depth * 0.4f)));

        Generator<MeshData> Result = RoomMesh;
        Result = Merge(Result, DeskMesh);
        Result = Merge(Result, ChairMesh);
        Result = Merge(Result, ComputerMesh);
        Result = Merge(Result, MonitorMesh);
        Result = Merge(Result, FilingCabinetMesh);

        return Result(SeedParamInner);
    });
}

Generator<MeshData> LabRoom(const Seed& SeedParam, float Size) {
    return Generator<MeshData>([SeedParam, Size](const Seed& SeedParamInner) {
        float Width = 5.0f * Size;
        float Depth = 4.0f * Size;
        float Height = 2.5f;

        // Room structure
        Generator<MeshData> RoomMesh = Room(SeedParamInner.Derive(0), Width, Depth, Height, 1);

        // Workbenches
        for (int i = 0; i < 2; ++i) {
            Generator<MeshData> TableMesh = Table(SeedParamInner.Derive(10 + i), 1.5f);
            TableMesh = Transform(TableMesh, Math::Matrix4::Translation(Math::Vec3(
                (i - 0.5f) * Width * 0.4f,
                0,
                -Depth * 0.3f
            )));
            RoomMesh = Merge(RoomMesh, TableMesh);
        }

        // Terminals
        for (int i = 0; i < 3; ++i) {
            Generator<MeshData> TerminalMesh = Terminal(SeedParamInner.Derive(20 + i), 0);
            TerminalMesh = Transform(TerminalMesh, Math::Matrix4::Translation(Math::Vec3(
                (i - 1) * Width * 0.3f,
                0.4f,
                Depth * 0.35f
            )));
            RoomMesh = Merge(RoomMesh, TerminalMesh);
        }

        return RoomMesh(SeedParamInner);
    });
}

Generator<MeshData> CorridorSection(const Seed& SeedParam, float Length, int Features) {
    return Generator<MeshData>([SeedParam, Length, Features](const Seed& SeedParamInner) {
        float Width = 2.0f;
        float Height = 2.5f;

        // Basic corridor
        Generator<MeshData> CorridorMesh = Corridor(SeedParamInner.Derive(0), Length, Width, Height);

        // Lights (if features include lighting)
        if (Features & 1) {
            int LightCount = static_cast<int>(Length / 3.0f);
            for (int i = 0; i < LightCount; ++i) {
                float x = (i / static_cast<float>(LightCount - 1)) * Length - Length * 0.5f;
                Generator<MeshData> LightMesh = Light(SeedParamInner.Derive(10 + i), 0);
                LightMesh = Transform(LightMesh, Math::Matrix4::Translation(Math::Vec3(x, Height - 0.1f, 0)));
                CorridorMesh = Merge(CorridorMesh, LightMesh);
            }
        }

        // Doors (if features include doors)
        if (Features & 2) {
            int DoorCount = static_cast<int>(Length / 5.0f);
            for (int i = 0; i < DoorCount; ++i) {
                float x = (i / static_cast<float>(DoorCount - 1)) * Length - Length * 0.5f;
                Generator<MeshData> DoorMesh = Door(SeedParamInner.Derive(20 + i), 0.9f, 2.0f, 0);
                DoorMesh = Transform(DoorMesh, Math::Matrix4::Translation(Math::Vec3(x, Height * 0.5f, Width * 0.5f)));
                CorridorMesh = Merge(CorridorMesh, DoorMesh);
            }
        }

        return CorridorMesh(SeedParamInner);
    });
}

Generator<MeshData> Stairwell(const Seed& SeedParam, int Floors, int Type) {
    return Generator<MeshData>([SeedParam, Floors, Type](const Seed& SeedParamInner) {
        float Width = 2.0f;
        float FloorHeight = 3.0f;

        Generator<MeshData> Result = Generator<MeshData>([](const Seed&) { return MeshData(); });

            // Stairs for each floor
            for (int floor = 0; floor < Floors; ++floor) {
                float y = floor * FloorHeight;
                Generator<MeshData> StairsMesh = Stairs(SeedParamInner.Derive(floor), 15, Width, Type);
            StairsMesh = Transform(StairsMesh, Math::Matrix4::Translation(Math::Vec3(0, y, 0)));

            // Railings
            Generator<MeshData> LeftRailing = Railing(SeedParamInner.Derive(100 + floor * 2), 4.5f, 1);
            LeftRailing = Transform(LeftRailing, Math::Matrix4::Translation(Math::Vec3(-Width * 0.6f, y + 0.9f, 0)));

            Generator<MeshData> RightRailing = Railing(SeedParamInner.Derive(101 + floor * 2), 4.5f, 1);
            RightRailing = Transform(RightRailing, Math::Matrix4::Translation(Math::Vec3(Width * 0.6f, y + 0.9f, 0)));

            if (floor == 0) {
                Result = StairsMesh;
            } else {
                Result = Merge(Result, StairsMesh);
            }
            Result = Merge(Result, LeftRailing);
            Result = Merge(Result, RightRailing);
        }

        // Lighting
        for (int floor = 0; floor < Floors; ++floor) {
            float y = floor * FloorHeight + FloorHeight * 0.5f;
            Generator<MeshData> LightMesh = Light(SeedParamInner.Derive(200 + floor), 0);
            LightMesh = Transform(LightMesh, Math::Matrix4::Translation(Math::Vec3(0, y, 0)));
            Result = Merge(Result, LightMesh);
        }

        return Result(SeedParamInner);
    });
}

Generator<MeshData> ControlRoom(const Seed& SeedParam, float Size) {
    return Generator<MeshData>([SeedParam, Size](const Seed& SeedParamInner) {
        float Width = 6.0f * Size;
        float Depth = 5.0f * Size;
        float Height = 2.5f;

        // Room structure
        Generator<MeshData> RoomMesh = Room(SeedParamInner.Derive(0), Width, Depth, Height, 0);

        // Control panels along walls
        for (int i = 0; i < 3; ++i) {
            Generator<MeshData> Panel = ControlPanel(SeedParamInner.Derive(10 + i), 2.0f);
            float angle = (i / 3.0f) * 2.0f * 3.14159f;
            float x = std::cos(angle) * Width * 0.4f;
            float z = std::sin(angle) * Depth * 0.4f;
            Panel = Transform(Panel, Math::Matrix4::Translation(Math::Vec3(x, Height * 0.4f, z)));
            RoomMesh = Merge(RoomMesh, Panel);
        }

        // Monitors
        for (int i = 0; i < 4; ++i) {
            Generator<MeshData> MonitorMesh = Monitor(SeedParamInner.Derive(20 + i), 1.0f, 0);
            MonitorMesh = Transform(MonitorMesh, Math::Matrix4::Translation(Math::Vec3(
                (i % 2 - 0.5f) * Width * 0.3f,
                Height * 0.6f,
                (i / 2 - 0.5f) * Depth * 0.3f
            )));
            RoomMesh = Merge(RoomMesh, MonitorMesh);
        }

        // Terminals
        for (int i = 0; i < 2; ++i) {
            Generator<MeshData> TerminalMesh = Terminal(SeedParamInner.Derive(30 + i), 0);
            TerminalMesh = Transform(TerminalMesh, Math::Matrix4::Translation(Math::Vec3(
                (i - 0.5f) * Width * 0.4f,
                0.4f,
                -Depth * 0.35f
            )));
            RoomMesh = Merge(RoomMesh, TerminalMesh);
        }

        return RoomMesh(SeedParamInner);
    });
}

Generator<MeshData> StorageRoom(const Seed& SeedParam, float Size) {
    return Generator<MeshData>([SeedParam, Size](const Seed& SeedParamInner) {
        float Width = 4.0f * Size;
        float Depth = 4.0f * Size;
        float Height = 2.5f;

        // Room structure
        Generator<MeshData> RoomMesh = Room(SeedParamInner.Derive(0), Width, Depth, Height, 2);

        // Shelves/cabinets
        for (int i = 0; i < 4; ++i) {
            Generator<MeshData> CabinetMesh = Cabinet(SeedParamInner.Derive(10 + i), 0.8f);
            CabinetMesh = Transform(CabinetMesh, Math::Matrix4::Translation(Math::Vec3(
                (i % 2 - 0.5f) * Width * 0.4f,
                0,
                (i / 2 - 0.5f) * Depth * 0.4f
            )));
            RoomMesh = Merge(RoomMesh, CabinetMesh);
        }

        return RoomMesh(SeedParamInner);
    });
}

Generator<MeshData> HallwayJunction(const Seed& SeedParam, int Type) {
    return Generator<MeshData>([SeedParam, Type](const Seed& SeedParamInner) {
        float Width = 2.0f;
        float Height = 2.5f;
        float Length = 3.0f;

        Generator<MeshData> Result = Generator<MeshData>([](const Seed&) { return MeshData(); });

        if (Type == 0) { // T-junction
            // Main corridor
            Generator<MeshData> Main = Hallway(SeedParamInner.Derive(0), Length, Width, Height);

            // Side corridor
            Generator<MeshData> Side = Hallway(SeedParamInner.Derive(1), Length, Width, Height);
            Side = Transform(Side, Math::Matrix4::Translation(Math::Vec3(0, 0, Length * 0.5f)));
            Side = Rotate(Side, 1.57f, Math::Vec3(0, 1, 0));

            Result = Merge(Main, Side);
        } else if (Type == 1) { // Cross junction
            Generator<MeshData> North = Hallway(SeedParamInner.Derive(0), Length, Width, Height);
            Generator<MeshData> South = Hallway(SeedParamInner.Derive(1), Length, Width, Height);
            South = Transform(South, Math::Matrix4::Translation(Math::Vec3(0, 0, -Length)));
            South = Rotate(South, 3.14159f, Math::Vec3(0, 1, 0));

            Generator<MeshData> East = Hallway(SeedParamInner.Derive(2), Length, Width, Height);
            East = Transform(East, Math::Matrix4::Translation(Math::Vec3(Length * 0.5f, 0, 0)));
            East = Rotate(East, 1.57f, Math::Vec3(0, 1, 0));

            Generator<MeshData> West = Hallway(SeedParamInner.Derive(3), Length, Width, Height);
            West = Transform(West, Math::Matrix4::Translation(Math::Vec3(-Length * 0.5f, 0, 0)));
            West = Rotate(West, -1.57f, Math::Vec3(0, 1, 0));

            Result = Merge(North, South);
            Result = Merge(Result, East);
            Result = Merge(Result, West);
        } else { // L-junction
            Generator<MeshData> Main = Hallway(SeedParamInner.Derive(0), Length, Width, Height);
            Generator<MeshData> Side = Hallway(SeedParamInner.Derive(1), Length, Width, Height);
            Side = Transform(Side, Math::Matrix4::Translation(Math::Vec3(Length * 0.5f, 0, Length * 0.5f)));
            Side = Rotate(Side, 1.57f, Math::Vec3(0, 1, 0));

            Result = Merge(Main, Side);
        }

        return Result(SeedParamInner);
    });
}

std::vector<MeshData> CombineRooms(
    const std::vector<Generator<MeshData>>& Rooms,
    const std::vector<RoomConnection>& Connections,
    const Seed& SeedParam
) {
    std::vector<MeshData> Result;

    // Generate all rooms
    for (size_t i = 0; i < Rooms.size(); ++i) {
        Result.push_back(Rooms[i](SeedParam.Derive(static_cast<uint32_t>(i))));
    }

    // TODO: Apply connections (would require more complex mesh merging)
    // For now, just return the rooms

    return Result;
}

std::vector<AssetPlacement> PlaceAssets(
    const Generator<MeshData>& Room,
    const std::vector<std::string>& AssetTypes,
    const Seed& SeedParam
) {
    std::vector<AssetPlacement> Placements;

    // Generate room to get bounds
    MeshData RoomMesh = Room(SeedParam);

    // Place assets procedurally
    for (size_t i = 0; i < AssetTypes.size(); ++i) {
        AssetPlacement Placement;
        Placement.AssetType = AssetTypes[i];

        // Random position within room bounds (simplified)
        Seed PosSeed = SeedParam.Derive(static_cast<uint32_t>(i * 100));
        float x = (NextFloat(PosSeed) - 0.5f) * (RoomMesh.BoundsMax.x - RoomMesh.BoundsMin.x);
        float z = (NextFloat(PosSeed.Derive(1)) - 0.5f) * (RoomMesh.BoundsMax.z - RoomMesh.BoundsMin.z);
        float y = RoomMesh.BoundsMin.y;

        Placement.Position = Math::Vec3(x, y, z);
        Placement.Rotation = Math::Quaternion();
        Placement.Scale = 1.0f;

        Placements.push_back(Placement);
    }

    return Placements;
}

Generator<MeshData> GenerateFloor(
    const Seed& SeedParam,
    const std::vector<std::pair<std::string, Math::Vec3>>& RoomLayout
) {
    return Generator<MeshData>([SeedParam, RoomLayout](const Seed& SeedParamInner) {
        Generator<MeshData> Result = Generator<MeshData>([](const Seed&) { return MeshData(); });

        // Generate rooms at specified positions
        for (size_t i = 0; i < RoomLayout.size(); ++i) {
            const auto& [RoomType, Position] = RoomLayout[i];
            Generator<MeshData> Room;

            // Select room type (simplified - would use actual room generators)
            if (RoomType == "Office") {
                Room = OfficeRoom(SeedParamInner.Derive(static_cast<uint32_t>(i * 10)), 1.0f);
            } else if (RoomType == "Lab") {
                Room = LabRoom(SeedParamInner.Derive(static_cast<uint32_t>(i * 10)), 1.0f);
            } else {
                Room = OfficeRoom(SeedParamInner.Derive(static_cast<uint32_t>(i * 10)), 1.0f);
            }

            Room = Transform(Room, Math::Matrix4::Translation(Position));

            if (i == 0) {
                Result = Room;
            } else {
                Result = Merge(Result, Room);
            }
        }

        return Result(SeedParamInner);
    });
}

} // namespace Solstice::Arzachel

