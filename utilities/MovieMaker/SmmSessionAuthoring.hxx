#pragma once

#include "LibUI/AssetBrowser/AssetBrowser.hxx"
#include <Parallax/DevSessionAssetResolver.hxx>
#include <Parallax/ParallaxScene.hxx>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Smm::Authoring {

struct AssetDbEntry {
    uint64_t Hash{0};
    std::string PathHint;
    std::string Tags; ///< Comma-separated labels for the simplified database.
    bool IsProxy{false};
    /// When **IsProxy**, optional hash of the “final” asset this row stands in for (0 = unknown).
    uint64_t ResolvesToHash{0};
};

struct PrefabEntry {
    std::string Id;
    std::string DisplayName;
    std::string SchemaName;
    /// Semicolon-separated `Key=Value` (strings; Vec3 as `x,y,z`). TP1: manual typing only.
    std::string AttrPairs;
};

struct LipsyncLineStub {
    std::string Label;
    std::string Text;
    uint64_t StartTick{0};
    uint64_t EndTick{0};
    /// Scales `MouthOpen` in generated samples (0.25–2 is a sensible range).
    float Strength{1.f};
    /// 0 = legacy even word windows; 1 = vowel-centered sub-timing (MG placeholder); 2 = viseme keys for 3D actor.
    uint8_t PhoneticMode{0};
    /// Hard cap on generated **keyframes** (rail against huge ranges).
    uint32_t MaxKeyframes{2048u};
};

struct SessionState {
    std::vector<AssetDbEntry> AssetDb;
    std::vector<PrefabEntry> Prefabs;
    std::vector<LipsyncLineStub> LipsyncStubs;
};

std::filesystem::path AuthoringSidecarPathForProject(const std::filesystem::path& smmJsonPath);

/// Plain-text **TSV** sidecar (not `.prlx` / Parallax version). Keeps SMM `version` 1 in `.smm.json`.
bool LoadSessionAuthoring(const std::filesystem::path& path, SessionState& out, std::string* err = nullptr);
bool SaveSessionAuthoring(const std::filesystem::path& path, const SessionState& st, std::string* err = nullptr);

/// Rough “morpheme-like” tokenization (2002–03 style): alphanumerics + hyphens, split on other chars.
std::vector<std::string> ExtractMorphemeLikeTokens(const std::string& text);

/// Naïve vowel-based mouth opening 0..1 for a **word** token (old-game lipsync stand-in).
float MouthOpenHeuristic2002(std::string_view word);

struct LipsyncSample {
    uint64_t Tick{0};
    float MouthOpen{0.f};
};

/// Placeholder time curve: one sample per word boundary + endpoints (drives MG Depth if applied).
void BuildLipsyncPlaceholderSamples(const LipsyncLineStub& line, uint32_t ticksPerSecond, std::vector<LipsyncSample>& out);

/** Apply placeholder **Depth** keyframes to **MGTextElement** (track `Depth`); tiny variance so MG sort order wiggles. */
bool TryApplyLipsyncPlaceholderToMgText(const LipsyncLineStub& line, Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::MGIndex mgTextIndex, std::string& errOut);

/** Viseme timeline on **ActorElement**: channels `FacialVisemeId` + `FacialVisemeWeight` (see ParallaxTypes `kChannelFacial*`). */
bool TryApplyLipsyncVisemeToActor(const LipsyncLineStub& line, Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::ElementIndex actorElement, std::string& errOut);

bool InstantiatePrefab(const PrefabEntry& prefab, Solstice::Parallax::ParallaxScene& scene, std::string& errOut,
    int* outElementIndex = nullptr, Solstice::Parallax::MGIndex* outMGIndex = nullptr);

void DrawAuthoringSessionTab(const char* tabId, SessionState& session, Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::DevSessionAssetResolver& resolver, const std::vector<LibUI::AssetBrowser::Entry>& browser,
    int& assetListSelected, bool& sceneDirty, int& elementSelected, int& mgElementSelected, std::string& statusLine,
    const std::filesystem::path& smmJsonPath, bool compressPrlx, uint32_t timeTicks, uint32_t ticksPerSecond);

} // namespace Smm::Authoring
