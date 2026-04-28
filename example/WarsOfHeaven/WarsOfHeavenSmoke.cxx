#include "WarsOfHeavenSmoke.hxx"

#include "WohChaseCamera.hxx"
#include "WohHullDamageArzachel.hxx"
#include "WohSpaceNavigation.hxx"
#include "WohWarshipFactory.hxx"
#include "WohStarryBackground.hxx"
#include "WohCombatScoring.hxx"

#include <Solstice.hxx>
#include <Core/Relic/Relic.hxx>
#include <Core/Debug/Debug.hxx>
#include <Material/Material.hxx>
#include <Math/Vector.hxx>
#include <Physics/Integration/PhysicsSystem.hxx>
#include <Game/Networking/MultiplayerPresets.hxx>
#include <Render/Post/PostProcessing.hxx>
#include <imgui.h>

using namespace Solstice;
using namespace Solstice::Math;

namespace Solstice::WarsOfHeaven {

namespace {
constexpr int kScancodeW = 26;
constexpr int kScancodeA = 4;
constexpr int kScancodeD = 7;
constexpr int kScancodeH = 11;
constexpr int kScancodeM = 16;
constexpr int kScancodeF = 9;
} // namespace

WarsOfHeavenSmoke::WarsOfHeavenSmoke() = default;

WarsOfHeavenSmoke::~WarsOfHeavenSmoke() {
    Shutdown();
}

void WarsOfHeavenSmoke::SampleArzachelFragmentation() {
    const Arzachel::MeshData md = BuildKineticFragmentedShipMesh(Arzachel::Seed(0xC0FFEEu), 0.35f);
    m_FragmentationMeshVertexCount = md.Positions.size();
}

void WarsOfHeavenSmoke::ApplyStubGameplay() {
    Solstice::Smf::SmfMap map;
    Game::ApplySmfGameplayToEngine(map);

    std::vector<Game::CaptureVolumeRuntime> cap;
    {
        Game::CaptureVolumeRuntime z;
        z.Name = "Belt_Mining_Alpha";
        z.ZoneId = "ffa_alpha";
        z.BoundsMin = Vec3(-50.f, -20.f, -50.f);
        z.BoundsMax = Vec3(50.f, 20.f, 50.f);
        z.TimeToCaptureSec = 12.f;
        cap.push_back(std::move(z));
    }
    std::vector<Game::CelestialOccluderRuntime> occ;
    {
        Game::CelestialOccluderRuntime o;
        o.Name = "Planet_Keth";
        o.Center = Vec3(0.f, 0.f, -2000.f);
        o.Radius = 400.f;
        occ.push_back(std::move(o));
    }
    Game::MatchGameplayAuthoringCache::Instance().SetAuthoring(std::move(cap), std::move(occ), {}, {});

    m_GravityBodies = BuildGravityFromAuthoring(Game::MatchGameplayAuthoringCache::Instance().CelestialOccluders());
    {
        PlanetaryGravitySource sun;
        sun.Center = Vec3(0.0f, 0.0f, 0.0f);
        sun.Radius = 16.0f;
        sun.Gm = 220.0f;
        m_GravityBodies.insert(m_GravityBodies.begin(), sun);
    }

    m_AsteroidBelts.clear();
    {
        AsteroidBeltRegion belt;
        belt.BoundsMin = Vec3(-120.f, -60.f, -120.f);
        belt.BoundsMax = Vec3(120.f, 60.f, 120.f);
        belt.StealthScale = 0.42f;
        m_AsteroidBelts.push_back(belt);
    }

    m_PlayerShip = CreateWarship(m_Registry, 1u, 1, WarshipClass::Destroyer, Vec3(0.f, 0.f, 0.f), "Aegis-1");
    m_Match.SetPlayerTeam(1u, 1u);

    m_EnemyShips.clear();
    m_EnemyShips.push_back(
        CreateHostileWarship(m_Registry, 2u, 2, WarshipClass::Frigate, Vec3(55.f, 0.f, 30.f), "Hostile-Skirmisher"));
    m_EnemyShips.push_back(
        CreateHostileWarship(m_Registry, 3u, 2, WarshipClass::Frigate, Vec3(-40.f, 4.f, 50.f), "Hostile-Archer"));
    m_EnemyShips.push_back(
        CreateHostileWarship(m_Registry, 4u, 2, WarshipClass::Corvette, Vec3(20.f, -3.f, 75.f), "Hostile-Picket"));
    m_Match.SetPlayerTeam(2u, 2u);
    m_Match.SetPlayerTeam(3u, 2u);
    m_Match.SetPlayerTeam(4u, 2u);
}

void WarsOfHeavenSmoke::InitWindow() {
    auto w = std::make_unique<UI::Window>(960, 540, "Wars of Heaven");
    w->SetKeyCallback([this](int /*key*/, int scancode, int action, int /*mods*/) {
        if (action == 1 && scancode == 41) {
            RequestClose();
        }
    });
    SetWindow(std::move(w));
}

void WarsOfHeavenSmoke::PollShipInput(float deltaTime) {
    auto* win = GetWindow();
    if (win) {
        const bool mNow = win->IsKeyScanPressed(kScancodeM);
        if (mNow && !m_PrevMDown) {
            m_MainMenuOpen = !m_MainMenuOpen;
        }
        m_PrevMDown = mNow;
        const bool fNow = win->IsKeyScanPressed(kScancodeF);
        if (fNow && !m_PrevFDown) {
            for (ECS::EntityId e : m_EnemyShips) {
                if (e != 0 && m_Registry.Has<ShipHullHealth>(e)) {
                    m_Registry.Get<ShipHullHealth>(e).ApplyDamage(95.0f);
                }
            }
        }
        m_PrevFDown = fNow;
    }
    if (!win || m_PlayerShip == 0) {
        return;
    }
    if (!m_Registry.Has<WohShipKinematics>(m_PlayerShip)) {
        return;
    }
    auto& k = m_Registry.Get<WohShipKinematics>(m_PlayerShip);
    const float dt = deltaTime;
    if (win->IsKeyScanPressed(kScancodeA)) {
        k.YawDeg -= 55.0f * dt;
    }
    if (win->IsKeyScanPressed(kScancodeD)) {
        k.YawDeg += 55.0f * dt;
    }
    const bool hNow = win->IsKeyScanPressed(kScancodeH);
    if (hNow && !m_PrevHDown && m_Registry.Has<ShipHullHealth>(m_PlayerShip)) {
        m_Registry.Get<ShipHullHealth>(m_PlayerShip).ApplyDamage(12.0f);
    }
    m_PrevHDown = hNow;
}

void WarsOfHeavenSmoke::Initialize() {
    Solstice::Initialize();
    InitWindow();
    auto* win = GetWindow();
    if (!win) {
        return;
    }
    const auto fb = win->GetFramebufferSize();
    m_Renderer = std::make_unique<Render::SoftwareRenderer>(fb.first, fb.second, 16, win->NativeWindow());

    UI::UISystem::Instance().Initialize(win->NativeWindow());
    if (void* ctx = UI::UISystem::Instance().GetImGuiContext()) {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx));
    }

    m_MeshLibrary = std::make_unique<Render::MeshLibrary>();
    m_MaterialLibrary = std::make_unique<Core::MaterialLibrary>();
    m_Scene.SetMeshLibrary(m_MeshLibrary.get());
    m_Scene.SetMaterialLibrary(m_MaterialLibrary.get());

    m_Camera = Render::Camera(Vec3(0.f, 2.f, 8.f), Vec3(0.f, 1.f, 0.f), -90.f, 0.f);

    ApplyStubGameplay();
    SampleArzachelFragmentation();

    m_Scripts.SetMatchContext(&m_Match, &m_Leaderboard);
    m_Scripts.SetProgressionContext(&m_Achievements, &m_ScoreMult);
    m_ScoreMult.SetBase(1.0f);
    m_ScoreMult.SetSession(1.0f);
    if (!m_Scripts.Initialize("example/WarsOfHeaven/scripts", &m_Registry, &m_Scene, &Physics::PhysicsSystem::Instance(),
            &m_Camera)) {
        SIMPLE_LOG("WarsOfHeavenSmoke: script init failed (ok if path missing in dev env)");
    } else {
        m_ScriptRan = true;
    }
    if (Core::Relic::IsInitialized() && Core::Relic::GetVirtualTable() != nullptr) {
        SIMPLE_LOG("WarsOfHeaven: RELIC virtual table ready (LoadByHash decompresses entries on demand)");
    }

    m_Net.EnsureCoordinator(m_Registry, m_Scheduler, Game::MultiplayerPresets::GetWarsOfHeaven());
}

void WarsOfHeavenSmoke::Shutdown() {
    m_Net.Shutdown();
    UI::UISystem::Instance().Shutdown();
    m_Renderer.reset();
    m_MaterialLibrary.reset();
    m_MeshLibrary.reset();
    Solstice::Shutdown();
}

void WarsOfHeavenSmoke::Update(float deltaTime) {
    m_Scheduler.ExecuteAll(m_Registry, deltaTime);

    m_LastFrameDelta = deltaTime;
    PollShipInput(deltaTime);
    m_GameTime += static_cast<double>(deltaTime);
    m_Scripts.Update(m_MainMenuOpen ? 0.0f : deltaTime);

    if (m_MainMenuOpen) {
        return;
    }

    std::vector<ECS::EntityId> hazardShips;
    if (m_PlayerShip != 0) {
        hazardShips.push_back(m_PlayerShip);
    }
    hazardShips.insert(hazardShips.end(), m_EnemyShips.begin(), m_EnemyShips.end());
    ApplyCelestialLethalHazards(m_Registry, hazardShips, deltaTime, m_GravityBodies);

    const bool forward = GetWindow() && GetWindow()->IsKeyScanPressed(kScancodeW);
    if (m_PlayerShip != 0) {
        UpdatePlayerShipIfPresent(m_Registry, m_PlayerShip, deltaTime, forward, m_GravityBodies, m_AsteroidBelts);
        if (m_Registry.Has<ECS::Transform>(m_PlayerShip) && m_Registry.Has<WohShipKinematics>(m_PlayerShip)) {
            const auto& t = m_Registry.Get<ECS::Transform>(m_PlayerShip);
            const auto& k = m_Registry.Get<WohShipKinematics>(m_PlayerShip);
            ApplyChaseCamera(m_Camera, t.Position, k, 28.0f, 6.0f);
        }
    }

    UpdateMatchCombatScoring(m_Registry, m_CombatState, deltaTime, m_GameTime, m_EnemyShips, m_PlayerShip, 1u, 1u, 2u, m_Match, m_Achievements, m_ScoreMult);
}

void WarsOfHeavenSmoke::HandleInput() {}

void WarsOfHeavenSmoke::Render() {
    if (!m_Renderer) {
        return;
    }
    UI::UISystem::Instance().NewFrame();
    m_Renderer->Clear(Vec4(0.02f, 0.02f, 0.05f, 1.f));
    if (Render::PostProcessing* pp = m_Renderer->GetPostProcessing()) {
        Render::PostProcessing::PostChainTunables t;
        t.PrenormBlurMix = m_MainMenuOpen ? 0.92f : 0.0f;
        pp->SetPostChainTunables(t);
    }
    m_Renderer->RenderScene(m_Scene, m_Camera, m_Lights);

    DrawWohStarryMenuBackground();
    m_Scripts.SetScriptTimeDelta(m_LastFrameDelta);
    (void)m_Scripts.RunModuleExport("WoH_UI", "OnFrame");

    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Wars of Heaven — dev")) {
        const auto& cap = Game::MatchGameplayAuthoringCache::Instance().CaptureVolumes();
        const auto& occ = Game::MatchGameplayAuthoringCache::Instance().CelestialOccluders();
        ImGui::Text("Match authoring cache: %zu capture volume(s), %zu occluder(s)", cap.size(), occ.size());
        m_Leaderboard.Refresh(m_Match);
        m_TeamBoard.Refresh(m_Match);
        ImGui::Separator();
        ImGui::Text("Leaderboard (player)");
        for (std::size_t i = 0; i < m_Leaderboard.GetRows().size() && i < 8; ++i) {
            const auto& r = m_Leaderboard.GetRows()[i];
            ImGui::Text("  [%zu] player %u  score %lld", i, r.MatchPlayerId, static_cast<long long>(r.Score));
        }
        ImGui::Separator();
        ImGui::Text("Teams");
        for (std::size_t t = 0; t < m_TeamBoard.GetRows().size() && t < 6; ++t) {
            const auto& r = m_TeamBoard.GetRows()[t];
            ImGui::Text("  team %u  score %lld", static_cast<unsigned>(r.TeamId), static_cast<long long>(r.Score));
        }
        ImGui::Separator();
        {
            static char s_HostAddress[96] = "127.0.0.1";
            static int s_Port = 27050;
            ImGui::Text("LAN (GNS) —");
            ImGui::InputText("Address", s_HostAddress, sizeof(s_HostAddress));
            ImGui::InputInt("Port", &s_Port);
            if (s_Port < 1) {
                s_Port = 1;
            }
            if (s_Port > 65535) {
                s_Port = 65535;
            }
            if (ImGui::Button("Host (listen)")) {
                (void)m_Net.Host(static_cast<std::uint16_t>(s_Port));
            }
            ImGui::SameLine();
            if (ImGui::Button("Join")) {
                (void)m_Net.Connect(s_HostAddress, static_cast<std::uint16_t>(s_Port));
            }
            ImGui::SameLine();
            if (ImGui::Button("Disconnect")) {
                m_Net.Disconnect();
            }
            ImGui::TextWrapped("%s", m_Net.BuildStatusLine().c_str());
        }
        ImGui::Separator();
        ImGui::Text("Main menu: %s (M toggles sim + prenorm blur)", m_MainMenuOpen ? "ON" : "OFF");
        ImGui::Text("Score mult (effective): %.2f", static_cast<double>(m_ScoreMult.GetEffective()));
        ImGui::Text("Achievements: double=%d triple=%d multi=%d juggernaut=%d", m_Achievements.IsUnlocked("woh.kill.double") ? 1 : 0,
            m_Achievements.IsUnlocked("woh.kill.triple") ? 1 : 0, m_Achievements.IsUnlocked("woh.kill.multikill") ? 1 : 0,
            m_Achievements.IsUnlocked("woh.kill.juggernaut") ? 1 : 0);
        ImGui::Separator();
        ImGui::Text("Scripts ready: %s", m_ScriptRan ? "yes" : "no");
        ImGui::Separator();
        ImGui::Text("WASD thrust / turn | H = hull | M = menu + blur | F = damage hostiles (test) | Arzachel verts: %zu", m_FragmentationMeshVertexCount);
        if (m_PlayerShip != 0 && m_Registry.Has<ECS::Transform>(m_PlayerShip) && m_Registry.Has<ShipHullHealth>(m_PlayerShip)) {
            const auto& t = m_Registry.Get<ECS::Transform>(m_PlayerShip);
            const auto& h = m_Registry.Get<ShipHullHealth>(m_PlayerShip);
            const float st = StealthScaleFromAsteroidBelts(t.Position, m_AsteroidBelts);
            ImGui::Text("Hull %.0f / %.0f  stealth scale %.2f  frag 01: %.2f", h.Current, h.Max, st,
                h.ArzachelFragmentation01);
            if (!occ.empty()) {
                const bool blocked = IsOccludedByPlanet(m_EnemyObserverPos, t.Position, occ[0].Center, occ[0].Radius);
                ImGui::Text("Planet LOS from observer: %s", blocked ? "occluded" : "clear");
            }
        }
    }
    ImGui::End();

    UI::UISystem::Instance().Render();
    m_Renderer->Present();
}

} // namespace Solstice::WarsOfHeaven
