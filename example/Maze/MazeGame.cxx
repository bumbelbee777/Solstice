#include "MazeGame.hxx"

#include <Arzachel/MeshFactory.hxx>
#include <Core/Audio/Audio.hxx>
#include <Core/Debug/Debug.hxx>
#include <Game/FPS/FPSMovement.hxx>
#include <Game/Gameplay/PortalFactory.hxx>
#include <Game/Systems/AudioSystem.hxx>
#include <Game/Systems/PortalSystem.hxx>
#include <Material/Material.hxx>
#include <Physics/Dynamics/RigidBody.hxx>
#include <Physics/Lighting/LightSource.hxx>
#include <Render/PhysicsBridge.hxx>
#include <Entity/Transform.hxx>
#include <Entity/Name.hxx>
#include <Entity/Kind.hxx>
#include <Entity/Components/AudioComponents.hxx>
#include <UI/Core/UISystem.hxx>
#include <Solstice.hxx>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Solstice::MazeExp {

namespace {

[[nodiscard]] int GIdx(int x, int y, int W) {
    return x + y * W;
}

void CarveMaze(std::vector<uint8_t>& carve, int W, int H, int x, int y, Core::DeterministicStream& rng) {
    auto at = [&](int cx, int cy) -> uint8_t& {
        return carve[static_cast<size_t>(GIdx(cx, cy, W))];
    };

    at(x, y) = 1;

    int order[] = {0, 1, 2, 3};
    for (int i = 3; i > 0; --i) {
        const int j = rng.RangeInt(0, i + 1);
        std::swap(order[i], order[j]);
    }

    const int dx[] = {0, 0, 2, -2};
    const int dy[] = {2, -2, 0, 0};

    for (int k = 0; k < 4; ++k) {
        const int o = order[k];
        const int nx = x + dx[o];
        const int ny = y + dy[o];
        if (nx < 1 || ny < 1 || nx >= W - 1 || ny >= H - 1)
            continue;
        if (at(nx, ny))
            continue;
        at(x + dx[o] / 2, y + dy[o] / 2) = 1;
        CarveMaze(carve, W, H, nx, ny, rng);
    }
}

[[nodiscard]] bool IsCarved(const std::vector<uint8_t>& g, int W, int H, int x, int y) {
    if (x < 0 || y < 0 || x >= W || y >= H)
        return false;
    return g[static_cast<size_t>(GIdx(x, y, W))] != 0;
}

constexpr int kEsc = 27;

} // namespace

MazeGame::MazeGame(std::uint64_t masterSeed)
    : m_MasterSeed(masterSeed)
    , m_RngPortal(masterSeed, Core::StreamTagFourCC('P', 'R', 'T', 'L'))
    , m_RngProp(masterSeed, Core::StreamTagFourCC('P', 'R', 'P', 'S')) {}

MazeGame::~MazeGame() {
    Shutdown();
}

Math::Vec3 MazeGame::CellCenterWorld(int gi, int gj) const {
    return Math::Vec3(static_cast<float>(gi) * kCell, 0.0f, static_cast<float>(gj) * kCell);
}

void MazeGame::AddStaticBox(const Math::Vec3& center, const Math::Vec3& halfExtents, uint32_t materialId) {
    const Math::Quaternion q{};
    const Math::Vec3 scale(halfExtents.x * 2.0f, halfExtents.y * 2.0f, halfExtents.z * 2.0f);
    const Render::SceneObjectID oid =
        m_Scene.AddObject(m_CubeMeshId, center, q, scale, Render::ObjectType_Static);
    m_Scene.SetMaterial(oid, materialId);

    const ECS::EntityId eid = m_Registry.Create();
    ECS::Transform tr{};
    tr.Position = center;
    tr.Scale = Math::Vec3(1.f, 1.f, 1.f);
    tr.Matrix = Math::Matrix4::Translation(center);

    auto& rb = m_Registry.Add<Physics::RigidBody>(eid);
    rb.Position = center;
    rb.Rotation = q;
    rb.IsStatic = true;
    rb.SetMass(0.f);
    rb.Type = Physics::ColliderType::Box;
    rb.HalfExtents = halfExtents;
    rb.Friction = 0.9f;
    rb.Restitution = 0.f;
    rb.RenderObjectID = oid;

    m_Registry.Add<ECS::Transform>(eid, tr);
    m_Registry.Add<ECS::Name>(eid, ECS::Name{"MazeBrick"});
    m_Registry.Add<ECS::Kind>(eid, ECS::Kind{ECS::EntityKind::Environment});
}

void MazeGame::GenerateMaze() {
    Core::DeterministicStream mazeRng(m_MasterSeed,
        Core::StreamTagFourCC('M', 'A', 'Z', 'E'));

    m_GridW = 29;
    m_GridH = 29;
    m_GridCarve.assign(static_cast<size_t>(m_GridW * m_GridH), 0);
    CarveMaze(m_GridCarve, m_GridW, m_GridH, m_StartCell.first, m_StartCell.second, mazeRng);

    std::vector<int> dist(static_cast<size_t>(m_GridW * m_GridH), -1);
    std::deque<std::pair<int, int>> dq;
    dq.push_back(m_StartCell);
    dist[static_cast<size_t>(GIdx(m_StartCell.first, m_StartCell.second, m_GridW))] = 0;

    constexpr int nxo[] = {1, -1, 0, 0};
    constexpr int nyo[] = {0, 0, 1, -1};

    while (!dq.empty()) {
        const auto [cx, cy] = dq.front();
        dq.pop_front();
        const int base = dist[static_cast<size_t>(GIdx(cx, cy, m_GridW))];
        for (int k = 0; k < 4; ++k) {
            const int nx = cx + nxo[k];
            const int ny = cy + nyo[k];
            if (!IsCarved(m_GridCarve, m_GridW, m_GridH, nx, ny))
                continue;
            const int id = GIdx(nx, ny, m_GridW);
            if (dist[static_cast<size_t>(id)] >= 0)
                continue;
            dist[static_cast<size_t>(id)] = base + 1;
            dq.push_back({nx, ny});
        }
    }

    m_FarCell = m_StartCell;
    int best = -1;
    for (int j = 0; j < m_GridH; ++j) {
        for (int i = 0; i < m_GridW; ++i) {
            const int d = dist[static_cast<size_t>(GIdx(i, j, m_GridW))];
            if (d > best) {
                best = d;
                m_FarCell = {i, j};
            }
        }
    }

    const int R = 4;
    for (int dy = -R; dy <= R; ++dy) {
        for (int dx = -R; dx <= R; ++dx) {
            const int nx = m_FarCell.first + dx;
            const int ny = m_FarCell.second + dy;
            if (nx > 0 && ny > 0 && nx < m_GridW - 1 && ny < m_GridH - 1)
                m_GridCarve[static_cast<size_t>(GIdx(nx, ny, m_GridW))] = 1;
        }
    }

    m_SanctumWorldCenter = CellCenterWorld(m_FarCell.first, m_FarCell.second);

    m_RngPortal = Core::DeterministicStream(m_MasterSeed,
        Core::StreamTagFourCC('P', 'R', 'T', 'L'));
    m_RngProp = Core::DeterministicStream(m_MasterSeed,
        Core::StreamTagFourCC('P', 'R', 'P', 'S'));
}

void MazeGame::InstanciateGeometry() {
    const float wallT = 0.28f;
    const float halfH = kRoomHeight * 0.5f;
    const Math::Quaternion decoQ{};

    for (int j = 0; j < m_GridH; ++j) {
        for (int i = 0; i < m_GridW; ++i) {
            if (!IsCarved(m_GridCarve, m_GridW, m_GridH, i, j))
                continue;

            const Math::Vec3 c = CellCenterWorld(i, j);
            const bool checker = ((i + j) & 1) != 0;
            AddStaticBox(Math::Vec3(c.x, -0.12f, c.z), Math::Vec3(kCell * 0.5f + 0.02f, 0.12f, kCell * 0.5f + 0.02f),
                         checker ? m_MatFloorAlt : m_MatFloor);
            AddStaticBox(Math::Vec3(c.x, kRoomHeight + 0.05f, c.z),
                         Math::Vec3(kCell * 0.5f + 0.02f, 0.08f, kCell * 0.5f + 0.02f), m_MatCeil);

            if (!IsCarved(m_GridCarve, m_GridW, m_GridH, i + 1, j)) {
                AddStaticBox(Math::Vec3(c.x + kCell * 0.5f, halfH, c.z),
                             Math::Vec3(wallT, halfH, kCell * 0.5f + 0.02f), checker ? m_MatWall : m_MatWallAlt);
            }
            if (!IsCarved(m_GridCarve, m_GridW, m_GridH, i - 1, j)) {
                AddStaticBox(Math::Vec3(c.x - kCell * 0.5f, halfH, c.z),
                             Math::Vec3(wallT, halfH, kCell * 0.5f + 0.02f), checker ? m_MatWallAlt : m_MatWall);
            }
            if (!IsCarved(m_GridCarve, m_GridW, m_GridH, i, j + 1)) {
                AddStaticBox(Math::Vec3(c.x, halfH, c.z + kCell * 0.5f),
                             Math::Vec3(kCell * 0.5f + 0.02f, halfH, wallT), checker ? m_MatWall : m_MatWallAlt);
            }
            if (!IsCarved(m_GridCarve, m_GridW, m_GridH, i, j - 1)) {
                AddStaticBox(Math::Vec3(c.x, halfH, c.z - kCell * 0.5f),
                             Math::Vec3(kCell * 0.5f + 0.02f, halfH, wallT), checker ? m_MatWallAlt : m_MatWall);
            }

            // Sparse emissive markers make navigation feel intentional and less flat.
            const std::uint64_t markerHash =
                static_cast<std::uint64_t>(i * 73856093) ^ static_cast<std::uint64_t>(j * 19349663) ^ m_MasterSeed;
            if ((markerHash & 15ULL) == 0ULL) {
                const Math::Vec3 runePos(c.x, 1.95f, c.z + kCell * 0.46f);
                const Math::Vec3 runeScale(0.42f, 0.22f, 0.015f);
                const Render::SceneObjectID rid =
                    m_Scene.AddObject(m_CubeMeshId, runePos, decoQ, runeScale, Render::ObjectType_Static);
                m_Scene.SetMaterial(rid, m_MatRune);
            }
        }
    }

    // Global containment so players cannot leave/circumvent the generated maze shell.
    const float worldW = static_cast<float>(m_GridW) * kCell;
    const float worldH = static_cast<float>(m_GridH) * kCell;
    const Math::Vec3 worldCenter((static_cast<float>(m_GridW - 1) * kCell) * 0.5f,
        0.f,
        (static_cast<float>(m_GridH - 1) * kCell) * 0.5f);
    const float halfW = worldW * 0.5f + 8.0f;
    const float halfD = worldH * 0.5f + 8.0f;

    AddStaticBox(Math::Vec3(worldCenter.x, -2.0f, worldCenter.z),
                 Math::Vec3(halfW, 1.6f, halfD), m_MatFloorAlt);

    const float boundaryHalfH = 5.6f;
    const float boundaryTopY = boundaryHalfH;
    AddStaticBox(Math::Vec3(worldCenter.x + halfW, boundaryTopY, worldCenter.z),
                 Math::Vec3(0.6f, boundaryHalfH, halfD), m_MatWallAlt);
    AddStaticBox(Math::Vec3(worldCenter.x - halfW, boundaryTopY, worldCenter.z),
                 Math::Vec3(0.6f, boundaryHalfH, halfD), m_MatWallAlt);
    AddStaticBox(Math::Vec3(worldCenter.x, boundaryTopY, worldCenter.z + halfD),
                 Math::Vec3(halfW, boundaryHalfH, 0.6f), m_MatWallAlt);
    AddStaticBox(Math::Vec3(worldCenter.x, boundaryTopY, worldCenter.z - halfD),
                 Math::Vec3(halfW, boundaryHalfH, 0.6f), m_MatWallAlt);
    AddStaticBox(Math::Vec3(worldCenter.x, boundaryHalfH * 2.0f + 0.2f, worldCenter.z),
                 Math::Vec3(halfW, 0.4f, halfD), m_MatCeil);

    Physics::PhysicsSystem::Instance().GetBridge().SyncToReactPhysics3D();
}

void MazeGame::PlaceShortcutPortals() {
    struct Cell {
        int x;
        int y;
    };
    std::vector<Cell> pathCells;
    pathCells.reserve(512);
    for (int j = 1; j < m_GridH - 1; ++j) {
        for (int i = 1; i < m_GridW - 1; ++i) {
            if (IsCarved(m_GridCarve, m_GridW, m_GridH, i, j))
                pathCells.push_back({i, j});
        }
    }

    if (pathCells.size() < 4)
        return;

    const int pairCount = std::min(4, static_cast<int>(pathCells.size() / 18 + 1));
    for (int p = 0; p < pairCount; ++p) {
        Cell a{};
        Cell b{};
        bool ok = false;
        for (int attempt = 0; attempt < 160; ++attempt) {
            const size_t ia = static_cast<size_t>(m_RngPortal.NextU64() % pathCells.size());
            const size_t ib = static_cast<size_t>(m_RngPortal.NextU64() % pathCells.size());
            a = pathCells[ia];
            b = pathCells[ib];
            const int man = std::abs(a.x - b.x) + std::abs(a.y - b.y);
            if (man < 10)
                continue;
            ok = true;
            break;
        }
        if (!ok)
            continue;

        const Math::Vec3 ca = CellCenterWorld(a.x, a.y);
        const Math::Vec3 cb = CellCenterWorld(b.x, b.y);
        Math::Vec3 dir(cb.x - ca.x, 0.f, cb.z - ca.z);
        const float len = std::max(0.01f, dir.Magnitude());
        dir = dir * (1.0f / len);

        const float yawA = std::atan2(dir.x, dir.z);
        const Math::Vec3 pa(ca.x + dir.x * kCell * 0.38f, kRoomHeight * 0.45f, ca.z + dir.z * kCell * 0.38f);
        const Math::Matrix4 matA = Math::Matrix4::Translation(pa) * Math::Matrix4::RotationY(yawA);

        const float yawB = yawA + static_cast<float>(M_PI);
        const Math::Vec3 pb(cb.x - dir.x * kCell * 0.38f, kRoomHeight * 0.45f,
                            cb.z - dir.z * kCell * 0.38f);
        const Math::Matrix4 matB = Math::Matrix4::Translation(pb) * Math::Matrix4::RotationY(yawB);

        ECS::Portal spa{};
        Game::MakeInvisiblePortal(spa);
        spa.HalfWidth = 0.90f;
        spa.HalfHeight = 1.65f;
        spa.ShowVisual = true;
        spa.EmitParticles = true;
        spa.ParticleRate = 24.0f;
        spa.ParticleLifetime = 1.1f;
        spa.ColorR = 0.26f;
        spa.ColorG = 0.62f;
        spa.ColorB = 1.0f;
        spa.ColorA = 0.95f;

        ECS::Portal spb = spa;
        spb.ColorR = 0.98f;
        spb.ColorG = 0.48f;
        spb.ColorB = 0.82f;

        const ECS::EntityId eA = Game::CreatePortalEntityWithMatrix(m_Registry, pa, matA, spa,
            "MazeWarpA_" + std::to_string(p));
        const ECS::EntityId eB = Game::CreatePortalEntityWithMatrix(m_Registry, pb, matB, spb,
            "MazeWarpB_" + std::to_string(p));
        Game::LinkBidirectionalPortals(m_Registry, eA, eB);
    }
}

void MazeGame::PlaceFinaleLoop(const Math::Vec3& roomCenter, float verticalSpan) {
    const float floorY = 0.18f;
    const float ceilYWorld = floorY + std::max(6.f, verticalSpan);
    Game::CreateVerticalLoopPortalPair(m_Registry, Math::Vec3(roomCenter.x, floorY, roomCenter.z), ceilYWorld,
                                       kCell * 0.42f, kCell * 0.42f, "MazeVertLoop");
}

void MazeGame::PlaceSanctumDecor(const Math::Vec3& roomCenter) {
    const float cz = roomCenter.z - kCell * 1.2f;
    const Math::Quaternion decoQ{};

    // Sanctum architecture inspired by warm plaster + teal tile mood.
    AddStaticBox(Math::Vec3(roomCenter.x, 0.06f, cz - 0.1f), Math::Vec3(6.6f, 0.05f, 3.6f), m_MatTileA);
    AddStaticBox(Math::Vec3(roomCenter.x, 1.65f, cz - 3.5f), Math::Vec3(6.6f, 1.55f, 0.10f), m_MatPlaster);
    AddStaticBox(Math::Vec3(roomCenter.x - 6.5f, 1.4f, cz - 0.1f), Math::Vec3(0.1f, 1.30f, 3.4f), m_MatPlaster);
    AddStaticBox(Math::Vec3(roomCenter.x + 6.5f, 1.4f, cz - 0.1f), Math::Vec3(0.1f, 1.30f, 3.4f), m_MatPlaster);
    AddStaticBox(Math::Vec3(roomCenter.x, 3.20f, cz - 0.1f), Math::Vec3(6.6f, 0.07f, 3.6f), m_MatTrim);

    for (int tx = -12; tx <= 12; ++tx) {
        const float x = roomCenter.x + static_cast<float>(tx) * 0.52f;
        AddStaticBox(Math::Vec3(x, 0.12f, cz - 3.34f), Math::Vec3(0.22f, 0.06f, 0.08f), m_MatTrim);
        AddStaticBox(Math::Vec3(x, 0.12f, cz + 3.16f), Math::Vec3(0.22f, 0.06f, 0.08f), m_MatTrim);
    }

    AddStaticBox(Math::Vec3(roomCenter.x - 1.4f, 0.45f, cz + 0.8f), Math::Vec3(1.5f, 0.35f, 0.65f), m_MatCouch);
    AddStaticBox(Math::Vec3(roomCenter.x - 1.4f, 0.05f, cz + 0.8f), Math::Vec3(2.0f, 0.04f, 0.95f), m_MatTileB);

    const float lampX = roomCenter.x + 2.1f;
    const float lampZ = cz + 1.0f + static_cast<float>(m_RngProp.RangeInt(-1, 2)) * 0.15f;
    AddStaticBox(Math::Vec3(lampX, 0.72f, lampZ), Math::Vec3(0.08f, 0.68f, 0.08f), m_MatGold);
    AddStaticBox(Math::Vec3(lampX, 1.62f, lampZ), Math::Vec3(0.42f, 0.08f, 0.42f), m_MatLamp);

    const float px = roomCenter.x;
    const float py = 0.62f;
    const float pz = cz - 1.65f;

    // Procedural "modeled" upright piano silhouette (layered shell + details).
    AddStaticBox(Math::Vec3(px, py + 0.22f, pz), Math::Vec3(1.72f, 0.23f, 0.64f), m_MatWood);
    AddStaticBox(Math::Vec3(px, py + 0.85f, pz + 0.34f), Math::Vec3(1.78f, 0.54f, 0.16f), m_MatWoodDark);
    AddStaticBox(Math::Vec3(px, py + 1.18f, pz - 0.52f), Math::Vec3(1.74f, 0.18f, 0.05f), m_MatGold);
    AddStaticBox(Math::Vec3(px - 1.66f, py + 0.68f, pz + 0.03f), Math::Vec3(0.09f, 0.70f, 0.68f), m_MatWoodDark);
    AddStaticBox(Math::Vec3(px + 1.66f, py + 0.68f, pz + 0.03f), Math::Vec3(0.09f, 0.70f, 0.68f), m_MatWoodDark);
    AddStaticBox(Math::Vec3(px - 1.36f, py - 0.02f, pz + 0.22f), Math::Vec3(0.10f, 0.42f, 0.10f), m_MatWoodDark);
    AddStaticBox(Math::Vec3(px + 1.36f, py - 0.02f, pz + 0.22f), Math::Vec3(0.10f, 0.42f, 0.10f), m_MatWoodDark);

    // Rounded pedal assembly from Arzachel cylinder mesh.
    for (int p = -1; p <= 1; ++p) {
        const Math::Vec3 pedalPos(px + static_cast<float>(p) * 0.23f, py - 0.12f, pz - 0.38f);
        const Render::SceneObjectID pid =
            m_Scene.AddObject(m_CylinderMeshId, pedalPos, decoQ, Math::Vec3(0.03f, 0.16f, 0.03f), Render::ObjectType_Static);
        m_Scene.SetMaterial(pid, m_MatGold);
    }

    m_PianoKeyObjects.clear();
    const int whiteCount = 14;
    for (int k = 0; k < whiteCount; ++k) {
        const float tx = px - 1.22f + static_cast<float>(k) * 0.188f;
        const Math::Vec3 kp(tx, py + 0.86f, pz + 0.20f);
        const Math::Vec3 ksz(0.09f, 0.045f, 0.155f);
        const Render::SceneObjectID kid =
            m_Scene.AddObject(m_CubeMeshId, kp, decoQ, ksz, Render::ObjectType_Static);
        m_Scene.SetMaterial(kid, m_MatKey);
        m_PianoKeyObjects.push_back(kid);
    }

    for (int k = 0; k < whiteCount; ++k) {
        const int octavePos = k % 7;
        if (octavePos == 2 || octavePos == 6) {
            continue;
        }
        const float tx = px - 1.13f + static_cast<float>(k) * 0.188f;
        const Math::Vec3 bkPos(tx, py + 0.955f, pz + 0.31f);
        const Math::Vec3 bkSize(0.06f, 0.07f, 0.08f);
        const Render::SceneObjectID bid =
            m_Scene.AddObject(m_CubeMeshId, bkPos, decoQ, bkSize, Render::ObjectType_Static);
        m_Scene.SetMaterial(bid, m_MatKeyBlack);
    }

    // Warm backlights behind piano.
    for (int b = -2; b <= 2; ++b) {
        const Math::Vec3 glowPos(px + static_cast<float>(b) * 0.72f, py + 1.20f, pz - 0.62f);
        const Math::Vec3 glowScale(0.20f, 0.16f, 0.03f);
        const Render::SceneObjectID gid =
            m_Scene.AddObject(m_CubeMeshId, glowPos, decoQ, glowScale, Render::ObjectType_Static);
        m_Scene.SetMaterial(gid, m_MatLamp);
    }

    const ECS::EntityId zoneEntity = m_Registry.Create();
    {
        ECS::AcousticZoneVolume z{};
        z.Name = "SanctumZone";
        z.Min = Math::Vec3(roomCenter.x - 8.f, -0.5f, roomCenter.z - 8.f);
        z.Max = Math::Vec3(roomCenter.x + 10.f, kRoomHeight + 2.f, roomCenter.z + 8.f);
        z.ReverbWetness = 0.28f;
        z.ReverbPreset = static_cast<int>(Core::Audio::ReverbPresetType::Room);
        z.MusicPath = "assets/piano_music.wav";
        m_Registry.Add<ECS::AcousticZoneVolume>(zoneEntity, z);
        m_Registry.Add<ECS::Name>(zoneEntity, ECS::Name{"SanctumAcoustics"});
    }

    const std::filesystem::path musicPath{"assets/piano_music.wav"};
    if (std::filesystem::exists(musicPath)) {
        const ECS::EntityId muz = m_Registry.Create();
        ECS::Transform mtr{};
        mtr.Position = Math::Vec3(px, py + 0.5f, pz);
        mtr.Matrix = Math::Matrix4::Translation(mtr.Position);
        ECS::AudioSource src{};
        src.SoundPath = "assets/piano_music.wav";
        src.Loop = true;
        src.AutoPlay = true;
        src.Volume = 1.35f;
        src.MinDistance = 6.f;
        src.MaxDistance = 72.f;
        src.Focus = 0.75f;
        src.Immersion = 0.72f;
        src.IsCriticalCue = true;
        m_Registry.Add<ECS::Transform>(muz, mtr);
        m_Registry.Add<ECS::AudioSource>(muz, src);
        m_Registry.Add<ECS::Name>(muz, ECS::Name{"PianoRoomMusic"});
        m_Registry.Add<ECS::Kind>(muz, ECS::Kind{ECS::EntityKind::Environment});
    }
}

void MazeGame::UpdatePianoKeys(float dt) {
    (void)dt;
    m_GameplayTime += dt;
    for (size_t i = 0; i < m_PianoKeyObjects.size(); ++i) {
        const float phase = static_cast<float>(i) * 0.65f + m_MasterSeed * 0.0001f;
        const float dip =
            std::sin(m_GameplayTime * 8.5f + phase) * 0.22f + 0.08f * std::sin(m_GameplayTime * 2.8f + static_cast<float>(i));
        m_Scene.SetRotation(m_PianoKeyObjects[i], Math::Quaternion::FromEuler(-dip, 0.f, 0.f));
    }
    m_Scene.UpdateTransforms();
}

void MazeGame::BuildWorld() {
    GenerateMaze();
    InstanciateGeometry();
    // Keep only finale loop portal to avoid random corridor "wall phase" impressions.
    PlaceSanctumDecor(m_SanctumWorldCenter);
    PlaceFinaleLoop(m_SanctumWorldCenter, 11.f);

    const Math::Vec3 spawn =
        CellCenterWorld(m_StartCell.first, m_StartCell.second) + Math::Vec3(0.f, 2.05f, 0.f);
    SpawnPlayer(spawn);

    m_Camera.WorldUp = Math::Vec3(0.f, 1.f, 0.f);
    m_Camera.Up = Math::Vec3(0.f, 1.f, 0.f);
    m_Camera.Zoom = 72.f;
    m_Camera.Yaw = -90.f;
    m_Camera.Pitch = -2.f;
    m_Camera.Position = spawn;
    m_Camera.ProcessMouseMovement(0.f, 0.f, true);
}

void MazeGame::InitializeWindow() {
    auto window = std::make_unique<UI::Window>(1280, 720, "Solstice — Maze");
    window->SetKeyCallback([this](int key, int, int action, int) {
        if (action != 1)
            return;
        UI::Window* w = GetWindow();
        if (!w || !m_Renderer)
            return;
        if (key == kEsc && m_MouseLocked) {
            m_MouseLocked = false;
            w->SetRelativeMouse(false);
            w->SetCursorGrab(false);
            w->ShowCursor(true);
            m_Renderer->SetShowCrosshair(false);
        }
    });
    window->SetMouseButtonCallback([this](int button, int action, int) {
        UI::Window* w = GetWindow();
        if (!w || button != 2 || action != 1 || !m_Renderer)
            return;
        m_MouseLocked = !m_MouseLocked;
        w->SetRelativeMouse(m_MouseLocked);
        w->SetCursorGrab(m_MouseLocked);
        w->ShowCursor(!m_MouseLocked);
        m_Renderer->SetShowCrosshair(m_MouseLocked);
    });
    window->SetCursorPosCallback([this](double dx, double dy) {
        if (!m_MouseLocked || !GetWindow())
            return;
        m_Camera.ProcessMouseMovement(static_cast<float>(dx), static_cast<float>(-dy));
    });
    SetWindow(std::move(window));
}

void MazeGame::ConfigureScheduler() {
    m_Scheduler.Clear();

    m_Scheduler.Register(ECS::SystemPhase::Input, "MazeFPSIn", [this](ECS::Registry&, float) {
        if (!m_Window || !m_MouseLocked || m_PlayerEntity == 0)
            return;
        Math::Vec3 md{};
        if (m_Window->IsKeyScanPressed(26) || m_Window->IsKeyScanPressed(82))
            md.z += 1.f;
        if (m_Window->IsKeyScanPressed(22) || m_Window->IsKeyScanPressed(81))
            md.z -= 1.f;
        if (m_Window->IsKeyScanPressed(4) || m_Window->IsKeyScanPressed(80))
            md.x += 1.f;
        if (m_Window->IsKeyScanPressed(7) || m_Window->IsKeyScanPressed(79))
            md.x -= 1.f;
        const bool jmp = m_Window->IsKeyScanPressed(44);
        const bool crouch = m_Window->IsKeyScanPressed(29);
        Game::FPSMovementSystem::ProcessInput(m_Registry, m_PlayerEntity, md, jmp, crouch, false);
    });
    m_Scheduler.Register(ECS::SystemPhase::Simulation, "MazeFPSMove",
        [this](ECS::Registry&, float dt) { Game::FPSMovementSystem::Update(m_Registry, dt, m_Camera); });
    m_Scheduler.Register(ECS::SystemPhase::Simulation, "MazePhys", [this](ECS::Registry&, float dt) {
        auto& phy = Physics::PhysicsSystem::Instance();
        if (!phy.IsRunning() || !phy.IsBoundTo(m_Registry))
            phy.Start(m_Registry);
        phy.Update(dt);
    });
    static Game::AudioSystem sAud;
    static Game::PortalSystem sPor;
    m_Scheduler.Register(ECS::SystemPhase::Simulation, "MazeAud", sAud);
    m_Scheduler.Register(ECS::SystemPhase::Simulation, "MazePortal", sPor);
    m_Scheduler.Register(
        ECS::SystemPhase::Presentation, "PianoAnim", [this](ECS::Registry&, float dt) { UpdatePianoKeys(dt); });
}

void MazeGame::SpawnPlayer(const Math::Vec3& position) {
    if (m_PlayerEntity != 0)
        return;
    m_PlayerEntity = m_Registry.Create();

    Game::FPSMovement mv{};
    Game::FPSMovementSystem::ApplyPreset(mv, "Quake");
    mv.EnableBunnyHopping = false;
    mv.MaxGroundSpeed *= 0.5f;

    Math::Quaternion q{};
    auto& rb = m_Registry.Add<Physics::RigidBody>(m_PlayerEntity);
    rb.Position = position;
    rb.Rotation = q;
    rb.Type = Physics::ColliderType::Capsule;
    rb.CapsuleHeight = 1.65f;
    rb.CapsuleRadius = 0.26f;
    rb.SetMass(72.f);
    rb.IsStatic = false;
    rb.EnableCCD = true;
    rb.CCDMotionThreshold = 0.05f;
    rb.LinearDamping = 0.14f;
    rb.Friction = 0.35f;

    ECS::Transform tr{};
    tr.Position = position;
    tr.Scale = Math::Vec3(1.f, 1.f, 1.f);
    tr.Matrix = Math::Matrix4::Translation(position);

    m_Registry.Add<Game::FPSMovement>(m_PlayerEntity, mv);
    m_Registry.Add<ECS::Transform>(m_PlayerEntity, tr);
    m_Registry.Add<ECS::AudioListener>(m_PlayerEntity, ECS::AudioListener{});
    m_Registry.Add<ECS::Kind>(m_PlayerEntity, ECS::Kind{ECS::EntityKind::Player});
}

void MazeGame::Initialize() {
    Solstice::Initialize();
    InitializeWindow();
    UI::Window* window = GetWindow();
    if (!window) {
        SIMPLE_LOG("MazeGame: Window failed");
        return;
    }

    auto fb = window->GetFramebufferSize();
    m_Renderer = std::make_unique<Render::SoftwareRenderer>(fb.first, fb.second, 16, window->NativeWindow());
    m_Renderer->SetWireframe(false);
    m_Renderer->SetPhysicsRegistry(&m_Registry);
    m_Renderer->SetPortalDebugMode(false);

    Solstice::UI::UISystem::Instance().Initialize(window->NativeWindow());
    if (void* ctx = Solstice::UI::UISystem::Instance().GetImGuiContext())
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx));

    m_MeshLibrary = std::make_unique<Render::MeshLibrary>();
    m_MaterialLibrary = std::make_unique<Core::MaterialLibrary>();
    m_Scene.SetMeshLibrary(m_MeshLibrary.get());
    m_Scene.SetMaterialLibrary(m_MaterialLibrary.get());

    m_CubeMeshId = m_MeshLibrary->AddMesh(Solstice::Arzachel::MeshFactory::CreateCube(1.0f));
    m_PlaneMeshId = m_MeshLibrary->AddMesh(Solstice::Arzachel::MeshFactory::CreatePlane(8.f, 8.f));
    m_CylinderMeshId = m_MeshLibrary->AddMesh(Solstice::Arzachel::MeshFactory::CreateCylinder(0.5f, 1.0f, 20));

    m_MatWall = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MatWall)->SetAlbedoColor(Math::Vec3(0.58f, 0.32f, 0.14f), 0.58f);
    m_MatWallAlt = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MatWallAlt)->SetAlbedoColor(Math::Vec3(0.71f, 0.44f, 0.20f), 0.64f);

    m_MatFloor = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MatFloor)->SetAlbedoColor(Math::Vec3(0.07f, 0.46f, 0.47f), 0.44f);
    m_MatFloorAlt = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MatFloorAlt)->SetAlbedoColor(Math::Vec3(0.05f, 0.36f, 0.38f), 0.52f);

    m_MatCeil = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MatCeil)->SetAlbedoColor(Math::Vec3(0.14f, 0.28f, 0.29f), 0.70f);

    m_MatCouch = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MatCouch)->SetAlbedoColor(Math::Vec3(0.48f, 0.30f, 0.61f), 0.88f);

    m_MatWood = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MatWood)->SetAlbedoColor(Math::Vec3(0.50f, 0.27f, 0.11f), 0.32f);
    m_MatWoodDark = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MatWoodDark)->SetAlbedoColor(Math::Vec3(0.15f, 0.08f, 0.04f), 0.24f);

    m_MatLamp = m_MaterialLibrary->AddMaterial(Core::Materials::CreateEmissive(Math::Vec3(1.f, 0.93f, 0.74f),
        9.6f));

    m_MatWindow = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MatWindow)->SetAlbedoColor(Math::Vec3(0.09f, 0.18f, 0.44f), 0.08f);

    m_MatKey = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MatKey)->SetAlbedoColor(Math::Vec3(0.95f, 0.95f, 0.95f), 0.34f);
    m_MatKeyBlack = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MatKeyBlack)->SetAlbedoColor(Math::Vec3(0.03f, 0.03f, 0.04f), 0.16f);
    m_MatTrim = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MatTrim)->SetAlbedoColor(Math::Vec3(0.07f, 0.10f, 0.14f), 0.18f);
    m_MatGold = m_MaterialLibrary->AddMaterial(Core::Materials::CreateMetal(Math::Vec3(0.84f, 0.64f, 0.22f), 0.24f));
    m_MatRune = m_MaterialLibrary->AddMaterial(Core::Materials::CreateEmissive(Math::Vec3(0.22f, 0.45f, 0.95f), 4.2f));
    m_MatTileA = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MatTileA)->SetAlbedoColor(Math::Vec3(0.06f, 0.42f, 0.43f), 0.34f);
    m_MatTileB = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MatTileB)->SetAlbedoColor(Math::Vec3(0.03f, 0.30f, 0.32f), 0.42f);
    m_MatPlaster = m_MaterialLibrary->AddMaterial(Core::Materials::CreateDefault());
    m_MaterialLibrary->GetMaterial(m_MatPlaster)->SetAlbedoColor(Math::Vec3(0.71f, 0.46f, 0.22f), 0.76f);

    Physics::PhysicsSystem::Instance().Start(m_Registry);
    ConfigureScheduler();
    BuildWorld();

    Physics::LightSource key{};
    key.Position = Math::Vec3(m_SanctumWorldCenter.x + 3.f, kRoomHeight + 1.2f, m_SanctumWorldCenter.z - 2.f);
    key.Color = Math::Vec3(1.00f, 0.86f, 0.62f);
    key.Intensity = 2.65f;
    key.Hue = 0.f;
    key.Attenuation = 0.009f;
    key.Type = Physics::LightSource::LightType::Point;
    m_Lights.push_back(key);

    Physics::LightSource soft{};
    soft.Position =
        CellCenterWorld(m_StartCell.first, m_StartCell.second) + Math::Vec3(2.f, kRoomHeight + 0.9f, 6.f);
    soft.Color = Math::Vec3(0.42f, 0.50f, 0.92f);
    soft.Intensity = 1.05f;
    soft.Hue = 0.f;
    soft.Attenuation = 0.02f;
    soft.Type = Physics::LightSource::LightType::Point;
    m_Lights.push_back(soft);

    // Guide lights nudge players deeper into the maze and toward the sanctum.
    for (int n = 1; n <= 5; ++n) {
        const float t = static_cast<float>(n) / 6.0f;
        const Math::Vec3 p = CellCenterWorld(m_StartCell.first, m_StartCell.second) * (1.0f - t) +
            m_SanctumWorldCenter * t + Math::Vec3(0.f, kRoomHeight * 0.62f, 0.f);
        Physics::LightSource breadcrumb{};
        breadcrumb.Position = p;
        breadcrumb.Color = Math::Vec3(0.30f, 0.58f, 1.0f);
        breadcrumb.Intensity = 0.52f;
        breadcrumb.Hue = 0.f;
        breadcrumb.Attenuation = 0.030f;
        breadcrumb.Type = Physics::LightSource::LightType::Point;
        m_Lights.push_back(breadcrumb);
    }

    SIMPLE_LOG("Maze initialized seed=" + std::to_string(m_MasterSeed));
}

void MazeGame::Shutdown() {
    Physics::PhysicsSystem::Instance().Stop();
    Solstice::UI::UISystem::Instance().Shutdown();
    m_Renderer.reset();
    Solstice::Shutdown();
}

void MazeGame::Update(float dt) {
    m_Scheduler.ExecuteAll(m_Registry, dt);
}

void MazeGame::Render() {
    if (!m_Renderer)
        return;
    Solstice::UI::UISystem::Instance().NewFrame();
    Render::SyncPhysicsToScene(m_Registry, m_Scene);

    m_Renderer->Clear(Math::Vec4(0.03f, 0.035f, 0.055f, 1.f));

    ImGui::Begin("Maze", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Seed %llu", static_cast<unsigned long long>(m_MasterSeed));
    ImGui::Text("RMB: mouse look | ESC: release");
    ImGui::End();

    m_Renderer->RenderScene(m_Scene, m_Camera, m_Lights);

    Solstice::UI::UISystem::Instance().Render();
    m_Renderer->Present();
}

void MazeGame::HandleInput() {}

} // namespace Solstice::MazeExp

