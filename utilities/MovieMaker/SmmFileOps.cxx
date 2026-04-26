#include "SmmFileOps.hxx"
#include "SmmView.hxx"

#include "UtilityPluginAbi.hxx"
#include "UtilityPluginUi.hxx"

#include <Core/Relic/Decompress.hxx>
#include <Core/Relic/PathTable.hxx>
#include <Core/Relic/Reader.hxx>
#include <Core/Relic/Types.hxx>
#include <Core/Relic/Writer.hxx>

#include <Parallax/Parallax.hxx>
#include <Parallax/ParallaxScene.hxx>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <new>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Smm {
std::mutex g_PendingMutex;
std::vector<std::string> g_PendingImportPaths;
std::optional<std::string> g_PendingRelicImportPath;
std::optional<std::string> g_PendingRelicExportPath;

void QueuePath(std::string p) {
    std::lock_guard<std::mutex> lock(g_PendingMutex);
    g_PendingImportPaths.push_back(std::move(p));
}

void QueueRelicImportPath(std::string p) {
    std::lock_guard<std::mutex> lock(g_PendingMutex);
    g_PendingRelicImportPath = std::move(p);
}

void QueueRelicExportPath(std::string p) {
    std::lock_guard<std::mutex> lock(g_PendingMutex);
    g_PendingRelicExportPath = std::move(p);
}

void DrainImports(Solstice::Parallax::DevSessionAssetResolver& resolver, std::vector<LibUI::AssetBrowser::Entry>& browser,
    Solstice::MovieMaker::Workflow::SessionMilestones* milestones) {
    std::vector<std::string> paths;
    {
        std::lock_guard<std::mutex> lock(g_PendingMutex);
        paths.swap(g_PendingImportPaths);
    }
    for (const auto& p : paths) {
        if (const auto hImport = resolver.ImportFile(std::filesystem::path(p))) {
            if (milestones) {
                milestones->assetsImported = true;
            }
            LibUI::AssetBrowser::Entry e;
            e.DisplayName = std::filesystem::path(p).filename().string();
            e.Hash = *hImport;
            browser.push_back(std::move(e));
        }
    }
}

static bool SmmImportRelicIntoSession(const std::filesystem::path& relicPath,
    Solstice::Parallax::DevSessionAssetResolver& resolver, std::vector<LibUI::AssetBrowser::Entry>& browser,
    std::string& errOut) {
    using Solstice::Core::Relic::AssetHash;
    using Solstice::Core::Relic::DecompressAsset;
    using Solstice::Core::Relic::GetCompressionType;
    using Solstice::Core::Relic::OpenRelic;
    using Solstice::Core::Relic::ParsePathTableBlob;

    auto container = OpenRelic(relicPath);
    if (!container) {
        errOut = "OpenRelic failed.";
        return false;
    }
    std::unordered_map<AssetHash, std::string> hashToLogical;
    if (!container->PathTableBlob.empty()) {
        std::vector<std::pair<std::string, AssetHash>> rows;
        if (!ParsePathTableBlob(std::span<const std::byte>(container->PathTableBlob.data(), container->PathTableBlob.size()),
                rows)) {
            errOut = "Invalid RELIC path table.";
            return false;
        }
        for (const auto& pr : rows) {
            hashToLogical[pr.second] = std::filesystem::path(pr.first).filename().string();
        }
    }

    std::ifstream f(relicPath, std::ios::binary);
    if (!f) {
        errOut = "Cannot open RELIC file.";
        return false;
    }
    const uint64_t dataBase = container->Header.DataBlobOffset;
    for (const auto& entry : container->Manifest) {
        f.seekg(static_cast<std::streamoff>(dataBase + entry.DataOffset));
        std::vector<std::byte> raw(entry.CompressedSize);
        if (entry.CompressedSize > 0) {
            f.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(entry.CompressedSize));
            if (!f) {
                errOut = "Read failed in RELIC data blob.";
                return false;
            }
        }
        std::vector<std::byte> dec =
            DecompressAsset(raw, GetCompressionType(entry.Flags), entry.UncompressedSize);
        if (entry.UncompressedSize > 0 && dec.size() != entry.UncompressedSize) {
            errOut = "Decompress failed for a RELIC entry.";
            return false;
        }
        Solstice::Parallax::AssetData ad;
        ad.Bytes = std::move(dec);
        std::string logical;
        auto it = hashToLogical.find(entry.AssetHash);
        if (it != hashToLogical.end()) {
            logical = it->second;
        } else {
            char name[40]{};
            std::snprintf(name, sizeof(name), "%016llX.bin", static_cast<unsigned long long>(entry.AssetHash));
            logical = name;
        }
        resolver.Register(entry.AssetHash, std::move(ad), logical);
        LibUI::AssetBrowser::Entry be;
        be.DisplayName = logical;
        be.Hash = entry.AssetHash;
        browser.push_back(std::move(be));
    }
    return true;
}

static bool SmmExportRelicSession(const Solstice::Parallax::DevSessionAssetResolver& resolver,
    const std::filesystem::path& destRelic, std::string& errOut) {
    using Solstice::Core::Relic::AssetTypeTag;
    using Solstice::Core::Relic::CompressionType;
    using Solstice::Core::Relic::RelicWriteInput;
    using Solstice::Core::Relic::RelicWriteOptions;
    using Solstice::Core::Relic::WriteRelic;

    std::vector<RelicWriteInput> inputs;
    for (const auto& kv : resolver.GetStore()) {
        RelicWriteInput in{};
        in.Hash = kv.first;
        in.TypeTag = AssetTypeTag::Unknown;
        in.ClusterId = 0;
        in.Compression = CompressionType::LZ4;
        in.Uncompressed = kv.second.Bytes;
        for (const auto& pb : resolver.GetPathBindings()) {
            if (pb.second == kv.first) {
                in.LogicalPath = pb.first;
                break;
            }
        }
        inputs.push_back(std::move(in));
    }
    if (inputs.empty()) {
        errOut = "No assets in session to export.";
        return false;
    }
    RelicWriteOptions opt{};
    if (!WriteRelic(destRelic, std::move(inputs), opt)) {
        errOut = "WriteRelic failed.";
        return false;
    }
    return true;
}

void DrainRelicFileOps(Solstice::Parallax::DevSessionAssetResolver& resolver,
    std::vector<LibUI::AssetBrowser::Entry>& browser, std::string& smmLastError,
    Solstice::MovieMaker::Workflow::SessionMilestones* milestones) {
    std::optional<std::string> imp;
    std::optional<std::string> exp;
    {
        std::lock_guard<std::mutex> lock(g_PendingMutex);
        imp = std::move(g_PendingRelicImportPath);
        exp = std::move(g_PendingRelicExportPath);
    }
    if (imp) {
        std::string err;
        if (SmmImportRelicIntoSession(std::filesystem::path(*imp), resolver, browser, err)) {
            if (milestones) {
                milestones->assetsImported = true;
            }
            smmLastError.clear();
        } else {
            smmLastError = err;
        }
    }
    if (exp) {
        std::string err;
        if (SmmExportRelicSession(resolver, std::filesystem::path(*exp), err)) {
            smmLastError.clear();
        } else {
            smmLastError = err;
        }
    }
}

Solstice::UtilityPluginHost::UtilityPluginHost g_MovieMakerPlugins;
std::vector<std::pair<std::string, std::string>> g_MovieMakerPluginLoadErrors;

void LoadMovieMakerPlugins() {
    g_MovieMakerPlugins.UnloadAll();
    g_MovieMakerPluginLoadErrors.clear();
    const char* base = SDL_GetBasePath();
    std::filesystem::path dir = base ? std::filesystem::path(base) / "plugins" : std::filesystem::path("plugins");
    Solstice::UtilityPluginHost::PluginAbiSymbols abi{};
    abi.GetName = SOLSTICE_UTILITY_ABI_MOVIE_MAKER_GETNAME;
    abi.OnLoad = SOLSTICE_UTILITY_ABI_MOVIE_MAKER_ONLOAD;
    abi.OnUnload = SOLSTICE_UTILITY_ABI_MOVIE_MAKER_ONUNLOAD;
    g_MovieMakerPlugins.LoadAllFromDirectory(dir.string(), abi, g_MovieMakerPluginLoadErrors);
}

void MovieMakerPluginsDrawPanel(bool* pOpen) {
    Solstice::UtilityPluginHost::DrawPluginManagerWindow(g_MovieMakerPlugins, pOpen, "Plugins##SMM", "MovieMaker",
        g_MovieMakerPluginLoadErrors, [] { LoadMovieMakerPlugins(); });
}

void SmmOpenLibUiRecentPath(void* userData, const char* pathUtf8) {
    auto* t = static_cast<SmmImportPathTarget*>(userData);
    if (t && t->buf && pathUtf8 && pathUtf8[0] && t->cap > 0) {
        std::snprintf(t->buf, t->cap, "%s", pathUtf8);
    }
}

const char* ParallaxErrorLabel(Solstice::Parallax::ParallaxError e) {
    using Solstice::Parallax::ParallaxError;
    switch (e) {
    case ParallaxError::None:
        return "None";
    case ParallaxError::InvalidMagic:
        return "InvalidMagic";
    case ParallaxError::UnsupportedVersion:
        return "UnsupportedVersion";
    case ParallaxError::CorruptHeader:
        return "CorruptHeader";
    case ParallaxError::CorruptStringTable:
        return "CorruptStringTable";
    case ParallaxError::CorruptElementGraph:
        return "CorruptElementGraph";
    case ParallaxError::CorruptChannelData:
        return "CorruptChannelData";
    case ParallaxError::CorruptMotionGraphics:
        return "CorruptMotionGraphics";
    case ParallaxError::AssetResolutionFailed:
        return "AssetResolutionFailed";
    case ParallaxError::VMVersionMismatch:
        return "VMVersionMismatch";
    case ParallaxError::StreamingError:
        return "StreamingError";
    case ParallaxError::OutOfMemory:
        return "OutOfMemory";
    default:
        return "Unknown";
    }
}

Solstice::Parallax::ChannelIndex FindChannelForAttribute(Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::ElementIndex element, std::string_view attribute, Solstice::Parallax::AttributeType type) {
    const auto& channels = scene.GetChannels();
    for (size_t i = 0; i < channels.size(); ++i) {
        if (channels[i].Element == element && channels[i].AttributeName == attribute && channels[i].ValueType == type) {
            return static_cast<Solstice::Parallax::ChannelIndex>(i);
        }
    }
    return Solstice::Parallax::PARALLAX_INVALID_INDEX;
}

void PushRecentPath(std::vector<std::string>& recent, const std::string& p, size_t maxN) {
    auto it = std::find(recent.begin(), recent.end(), p);
    if (it != recent.end()) {
        recent.erase(it);
    }
    recent.insert(recent.begin(), p);
    if (recent.size() > maxN) {
        recent.resize(maxN);
    }
    LibUI::Core::RecentPathPush(p.c_str());
}


std::filesystem::path MovieMakerDefaultProjectPath() {
    const char* b = SDL_GetBasePath();
    if (b) {
        return std::filesystem::path(b) / "solstice_moviemaker_project.smm.json";
    }
    return std::filesystem::path("solstice_moviemaker_project.smm.json");
}

static std::string EscapeJsonFragment(std::string_view s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\\' || c == '"') {
            o += '\\';
        }
        o += c;
    }
    return o;
}

void SaveMovieMakerProjectToPath(const std::filesystem::path& path, const MovieMakerProjectState& st) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return;
    }
    out << "{\"version\":1,\"exportPath\":\"" << EscapeJsonFragment(st.exportPath) << "\",\"importPath\":\""
        << EscapeJsonFragment(st.importPath) << "\",\"folderPath\":\"" << EscapeJsonFragment(st.folderPath)
        << "\",\"ffmpegExe\":\"" << EscapeJsonFragment(st.ffmpegExe) << "\",\"videoExportPath\":\""
        << EscapeJsonFragment(st.videoExportPath) << "\",\"videoWidth\":" << st.videoWidth << ",\"videoHeight\":"
        << st.videoHeight << ",\"videoFps\":" << st.videoFps << ",\"videoMp4\":" << (st.videoMp4 ? "true" : "false")
        << ",\"compressPrlx\":" << (st.compressPrlx ? "true" : "false")
        << ",\"videoStartTick\":" << st.videoStartTick << ",\"videoEndTick\":" << st.videoEndTick
        << ",\"sessionNotes\":\"" << EscapeJsonFragment(st.sessionNotes) << "\",\"loopRegionEnabled\":"
        << (st.loopRegionEnabled ? "true" : "false") << ",\"loopRegionStartTick\":" << st.loopRegionStartTick
        << ",\"loopRegionEndTick\":" << st.loopRegionEndTick << ",\"playbackPermille\":" << st.playbackPermille
        << ",\"recent\":[";
    for (size_t i = 0; i < st.recentPrlx.size(); ++i) {
        if (i > 0) {
            out << ',';
        }
        out << "\"" << EscapeJsonFragment(st.recentPrlx[i]) << "\"";
    }
    out << "],\"markers\":[";
    for (size_t i = 0; i < st.timelineMarkers.size(); ++i) {
        if (i > 0) {
            out << ',';
        }
        out << "{\"tick\":" << st.timelineMarkers[i].first << ",\"label\":\"" << EscapeJsonFragment(st.timelineMarkers[i].second)
            << "\"}";
    }
    out << "]}\n";
}

bool ParseJsonKeyU32(const std::string& j, const char* key, uint32_t& out) {
    std::string pat = std::string("\"") + key + "\":";
    size_t p = j.find(pat);
    if (p == std::string::npos) {
        return false;
    }
    p += pat.size();
    while (p < j.size() && (j[p] == ' ' || j[p] == '\t')) {
        ++p;
    }
    uint64_t v = 0;
    while (p < j.size() && j[p] >= '0' && j[p] <= '9') {
        v = v * 10ull + static_cast<uint64_t>(j[p] - '0');
        ++p;
    }
    if (v > 0xffffffffull) {
        return false;
    }
    out = static_cast<uint32_t>(v);
    return true;
}

bool ParseJsonKeyU64(const std::string& j, const char* key, uint64_t& out) {
    std::string pat = std::string("\"") + key + "\":";
    size_t p = j.find(pat);
    if (p == std::string::npos) {
        return false;
    }
    p += pat.size();
    while (p < j.size() && (j[p] == ' ' || j[p] == '\t')) {
        ++p;
    }
    out = 0;
    while (p < j.size() && j[p] >= '0' && j[p] <= '9') {
        out = out * 10ull + static_cast<uint64_t>(j[p] - '0');
        ++p;
    }
    return true;
}

bool ParseJsonKeyBool(const std::string& j, const char* key, bool& out) {
    std::string pat = std::string("\"") + key + "\":";
    size_t p = j.find(pat);
    if (p == std::string::npos) {
        return false;
    }
    p += pat.size();
    while (p < j.size() && (j[p] == ' ' || j[p] == '\t')) {
        ++p;
    }
    if (j.compare(p, 4, "true") == 0) {
        out = true;
        return true;
    }
    if (j.compare(p, 5, "false") == 0) {
        out = false;
        return true;
    }
    return false;
}

bool ParseMovieMakerProjectJson(const std::string& j, MovieMakerProjectState& st) {
    auto getStr = [&](const char* key) -> std::optional<std::string> {
        std::string k = std::string("\"") + key + "\":\"";
        size_t pos = j.find(k);
        if (pos == std::string::npos) {
            return std::nullopt;
        }
        pos += k.size();
        std::string out;
        while (pos < j.size()) {
            char c = j[pos++];
            if (c == '"') {
                break;
            }
            if (c == '\\' && pos < j.size()) {
                c = j[pos++];
            }
            out += c;
        }
        return out;
    };
    if (auto v = getStr("exportPath")) {
        st.exportPath = std::move(*v);
    }
    if (auto v = getStr("importPath")) {
        st.importPath = std::move(*v);
    }
    if (auto v = getStr("folderPath")) {
        st.folderPath = std::move(*v);
    }
    if (auto v = getStr("ffmpegExe")) {
        st.ffmpegExe = std::move(*v);
    }
    if (auto v = getStr("videoExportPath")) {
        st.videoExportPath = std::move(*v);
    }
    uint32_t vw = st.videoWidth;
    if (ParseJsonKeyU32(j, "videoWidth", vw)) {
        st.videoWidth = vw;
    }
    uint32_t vh = st.videoHeight;
    if (ParseJsonKeyU32(j, "videoHeight", vh)) {
        st.videoHeight = vh;
    }
    uint32_t vf = st.videoFps;
    if (ParseJsonKeyU32(j, "videoFps", vf)) {
        st.videoFps = vf;
    }
    bool vmp4 = st.videoMp4;
    if (ParseJsonKeyBool(j, "videoMp4", vmp4)) {
        st.videoMp4 = vmp4;
    }
    uint64_t vst = st.videoStartTick;
    if (ParseJsonKeyU64(j, "videoStartTick", vst)) {
        st.videoStartTick = vst;
    }
    uint64_t vet = st.videoEndTick;
    if (ParseJsonKeyU64(j, "videoEndTick", vet)) {
        st.videoEndTick = vet;
    }
    bool cpr = st.compressPrlx;
    if (ParseJsonKeyBool(j, "compressPrlx", cpr)) {
        st.compressPrlx = cpr;
    }
    st.recentPrlx.clear();
    size_t rpos = j.find("\"recent\":[");
    if (rpos != std::string::npos) {
        rpos += 10;
        while (rpos < j.size() && j[rpos] != ']') {
            if (j[rpos] == '"') {
                ++rpos;
                std::string item;
                while (rpos < j.size()) {
                    char c = j[rpos++];
                    if (c == '"') {
                        break;
                    }
                    if (c == '\\' && rpos < j.size()) {
                        c = j[rpos++];
                    }
                    item += c;
                }
                if (!item.empty()) {
                    st.recentPrlx.push_back(std::move(item));
                }
            } else {
                ++rpos;
            }
        }
    }
    if (auto sn = getStr("sessionNotes")) {
        st.sessionNotes = std::move(*sn);
    }
    bool lr = st.loopRegionEnabled;
    if (ParseJsonKeyBool(j, "loopRegionEnabled", lr)) {
        st.loopRegionEnabled = lr;
    }
    uint64_t ls = st.loopRegionStartTick;
    if (ParseJsonKeyU64(j, "loopRegionStartTick", ls)) {
        st.loopRegionStartTick = ls;
    }
    uint64_t le = st.loopRegionEndTick;
    if (ParseJsonKeyU64(j, "loopRegionEndTick", le)) {
        st.loopRegionEndTick = le;
    }
    uint32_t pp = st.playbackPermille;
    if (ParseJsonKeyU32(j, "playbackPermille", pp)) {
        st.playbackPermille = (std::max)(1u, (std::min)(10000u, pp));
    }
    st.timelineMarkers.clear();
    size_t mp = j.find("\"markers\":[");
    if (mp != std::string::npos) {
        mp += 11;
        while (mp < j.size()) {
            while (mp < j.size() && (j[mp] == ' ' || j[mp] == '\t' || j[mp] == '\n' || j[mp] == '\r' || j[mp] == ',')) {
                ++mp;
            }
            if (mp >= j.size() || j[mp] == ']') {
                break;
            }
            if (j[mp] != '{') {
                break;
            }
            const size_t tickKey = j.find("\"tick\":", mp);
            if (tickKey == std::string::npos) {
                break;
            }
            size_t q = tickKey + 7;
            while (q < j.size() && (j[q] == ' ' || j[q] == '\t')) {
                ++q;
            }
            uint64_t tickV = 0;
            while (q < j.size() && j[q] >= '0' && j[q] <= '9') {
                tickV = tickV * 10ull + static_cast<uint64_t>(j[q] - '0');
                ++q;
            }
            const size_t labKey = j.find("\"label\":\"", q);
            if (labKey == std::string::npos) {
                break;
            }
            size_t r = labKey + 9;
            std::string lab;
            while (r < j.size()) {
                char c = j[r++];
                if (c == '"') {
                    break;
                }
                if (c == '\\' && r < j.size()) {
                    c = j[r++];
                    if (c == 'n') {
                        c = '\n';
                    }
                }
                lab += c;
            }
            st.timelineMarkers.push_back({tickV, std::move(lab)});
            mp = j.find('}', r);
            if (mp == std::string::npos) {
                break;
            }
            ++mp;
        }
    }
    return true;
}

bool LoadMovieMakerProjectFromPath(const std::filesystem::path& path, MovieMakerProjectState& st) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream oss;
    oss << in.rdbuf();
    return ParseMovieMakerProjectJson(oss.str(), st);
}

std::mutex g_ProjectMutex;
std::optional<MovieMakerProjectState> g_PendingProjectApply;

#ifdef _WIN32
void CopyAsciiToSystemClipboard(const std::string& s) {
    if (!OpenClipboard(nullptr)) {
        return;
    }
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, s.size() + 1);
    if (h) {
        void* p = GlobalLock(h);
        if (p) {
            std::memcpy(p, s.c_str(), s.size() + 1);
            GlobalUnlock(h);
            SetClipboardData(CF_TEXT, h);
        }
    }
    CloseClipboard();
}
#endif

void ImportFolderRecursive(const std::filesystem::path& dir, Solstice::Parallax::DevSessionAssetResolver& resolver,
    std::vector<LibUI::AssetBrowser::Entry>& browser, Solstice::MovieMaker::Workflow::SessionMilestones* milestones) {
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (const auto hF = resolver.ImportFile(entry.path())) {
            if (milestones) {
                milestones->assetsImported = true;
            }
            LibUI::AssetBrowser::Entry e;
            e.DisplayName = entry.path().filename().string();
            e.Hash = *hF;
            browser.push_back(std::move(e));
        }
    }
}

const LibUI::FileDialogs::FileFilter kSmmProjectFilters[] = {
    {"MovieMaker project", "json"},
    {"All", "*"},
};

const LibUI::FileDialogs::FileFilter kRelicFilters[] = {
    {"RELIC archive", "relic"},
    {"All", "*"},
};

void ApplyMovieMakerProjectState(MovieMakerProjectState& st, char* exportPathBuf, size_t exportPathBufSize,
    char* importPathBuf, size_t importPathBufSize, char* folderPathBuf, size_t folderPathBufSize, char* ffmpegExeBuf,
    size_t ffmpegExeBufSize, char* videoExportPathBuf, size_t videoExportPathBufSize, uint32_t& videoW, uint32_t& videoH,
    uint32_t& videoFps, bool& videoMp4, uint64_t& videoStartTick, uint64_t& videoEndTick, bool& compressPrlx,
    std::vector<std::string>& recentPrlxPaths, std::filesystem::path& activeProjectPath, char* sessionNotesBuf,
    size_t sessionNotesBufSize, std::vector<std::pair<uint64_t, std::string>>* timelineMarkersOut,
    bool& loopRegionEnabled, uint64_t& loopRegionStartTick, uint64_t& loopRegionEndTick, uint32_t& playbackPermilleOut) {
    std::snprintf(exportPathBuf, exportPathBufSize, "%s", st.exportPath.c_str());
    std::snprintf(importPathBuf, importPathBufSize, "%s", st.importPath.c_str());
    std::snprintf(folderPathBuf, folderPathBufSize, "%s", st.folderPath.c_str());
    std::snprintf(ffmpegExeBuf, ffmpegExeBufSize, "%s", st.ffmpegExe.c_str());
    std::snprintf(videoExportPathBuf, videoExportPathBufSize, "%s", st.videoExportPath.c_str());
    videoW = st.videoWidth;
    videoH = st.videoHeight;
    videoFps = st.videoFps;
    videoMp4 = st.videoMp4;
    videoStartTick = st.videoStartTick;
    videoEndTick = st.videoEndTick;
    compressPrlx = st.compressPrlx;
    recentPrlxPaths = std::move(st.recentPrlx);
    if (st.loadedFromPath.has_value()) {
        activeProjectPath = std::filesystem::path(*st.loadedFromPath);
    }
    if (sessionNotesBuf && sessionNotesBufSize > 0) {
        std::snprintf(sessionNotesBuf, sessionNotesBufSize, "%s", st.sessionNotes.c_str());
    }
    if (timelineMarkersOut) {
        *timelineMarkersOut = st.timelineMarkers;
    }
    loopRegionEnabled = st.loopRegionEnabled;
    loopRegionStartTick = st.loopRegionStartTick;
    loopRegionEndTick = st.loopRegionEndTick;
    playbackPermilleOut = st.playbackPermille;
}

void DrainPendingMovieMakerProject(char* exportPathBuf, size_t exportPathBufSize, char* importPathBuf,
    size_t importPathBufSize, char* folderPathBuf, size_t folderPathBufSize, char* ffmpegExeBuf, size_t ffmpegExeBufSize,
    char* videoExportPathBuf, size_t videoExportPathBufSize, uint32_t& videoW, uint32_t& videoH, uint32_t& videoFps,
    bool& videoMp4, uint64_t& videoStartTick, uint64_t& videoEndTick, bool& compressPrlx,
    std::vector<std::string>& recentPrlxPaths, std::filesystem::path& activeProjectPath, char* sessionNotesBuf,
    size_t sessionNotesBufSize, std::vector<std::pair<uint64_t, std::string>>* timelineMarkersOut,
    bool& loopRegionEnabled, uint64_t& loopRegionStartTick, uint64_t& loopRegionEndTick, uint32_t& playbackPermilleOut) {
    std::optional<MovieMakerProjectState> pending;
    {
        std::lock_guard<std::mutex> lock(g_ProjectMutex);
        pending = std::move(g_PendingProjectApply);
    }
    if (pending) {
        ApplyMovieMakerProjectState(*pending, exportPathBuf, exportPathBufSize, importPathBuf, importPathBufSize,
            folderPathBuf, folderPathBufSize, ffmpegExeBuf, ffmpegExeBufSize, videoExportPathBuf, videoExportPathBufSize,
            videoW, videoH, videoFps, videoMp4, videoStartTick, videoEndTick, compressPrlx, recentPrlxPaths,
            activeProjectPath, sessionNotesBuf, sessionNotesBufSize, timelineMarkersOut, loopRegionEnabled,
            loopRegionStartTick, loopRegionEndTick, playbackPermilleOut);
    }
}

LibUI::Undo::SnapshotStack<std::vector<std::byte>> g_sceneByteUndo{kSceneUndoMax};

namespace {
char g_PendingUndoSnapshotMessage[256]{};
} // namespace

void ClearSceneUndo() { g_sceneByteUndo.Clear(); }

const char* GetPendingUndoSnapshotMessage() {
    return g_PendingUndoSnapshotMessage[0] != '\0' ? g_PendingUndoSnapshotMessage : nullptr;
}

void ClearPendingUndoSnapshotMessage() {
    g_PendingUndoSnapshotMessage[0] = '\0';
}

void PushSceneUndoSnapshot(const Solstice::Parallax::ParallaxScene& scene, bool compressPrlx) {
    std::vector<std::byte> bytes;
    Solstice::Parallax::ParallaxError err = Solstice::Parallax::ParallaxError::None;
    if (!Solstice::Parallax::SaveSceneToBytes(scene, bytes, compressPrlx, &err)) {
        return;
    }
    try {
        g_sceneByteUndo.PushBeforeChange(std::move(bytes));
    } catch (const std::bad_alloc&) {
        (void)err;
        std::snprintf(g_PendingUndoSnapshotMessage, sizeof(g_PendingUndoSnapshotMessage), "%s",
            "Undo snapshot was skipped (out of memory). Try saving; reduce scene size, or free memory.");
    }
}

bool SceneUndoRedoApply(Solstice::Parallax::ParallaxScene& scene, bool compressPrlx, bool undoNotRedo, int& elementSelected,
    uint64_t& timeTicks, bool loopRegionEnabled, uint64_t loopRegionStartTick, uint64_t loopRegionEndTick) {
    std::vector<std::byte> wire;
    Solstice::Parallax::ParallaxError err = Solstice::Parallax::ParallaxError::None;
    if (!Solstice::Parallax::SaveSceneToBytes(scene, wire, compressPrlx, &err)) {
        return false;
    }
    const bool ok = undoNotRedo ? g_sceneByteUndo.Undo(wire) : g_sceneByteUndo.Redo(wire);
    if (!ok) {
        return false;
    }
    if (!Solstice::Parallax::LoadSceneFromBytes(scene, wire, &err)) {
        return false;
    }
    if (scene.GetElements().empty()) {
        elementSelected = -1;
    } else {
        elementSelected = (std::min)(elementSelected, static_cast<int>(scene.GetElements().size()) - 1);
        if (elementSelected < 0) {
            elementSelected = 0;
        }
    }
    timeTicks = Solstice::MovieMaker::Workflow::ClampPlayheadWithLoop(timeTicks, scene.GetTimelineDurationTicks(),
        loopRegionEnabled, loopRegionStartTick, loopRegionEndTick);
    return true;
}

void JumpPlayheadToStartWithLoop(uint64_t& playhead, uint64_t timelineDurationTicks, bool loopEnabled,
    uint64_t loopStartTick, uint64_t loopEndTick) {
    if (loopEnabled && loopEndTick > loopStartTick) {
        playhead = Solstice::MovieMaker::Workflow::ClampPlayhead(loopStartTick, timelineDurationTicks);
    } else {
        Solstice::MovieMaker::Workflow::JumpPlayheadToStart(playhead, timelineDurationTicks);
    }
}

void JumpPlayheadToEndWithLoop(uint64_t& playhead, uint64_t timelineDurationTicks, bool loopEnabled,
    uint64_t loopStartTick, uint64_t loopEndTick) {
    if (loopEnabled && loopEndTick > loopStartTick) {
        uint64_t le = loopEndTick;
        if (timelineDurationTicks > 0 && le > timelineDurationTicks) {
            le = timelineDurationTicks;
        }
        playhead = le;
    } else {
        Solstice::MovieMaker::Workflow::JumpPlayheadToEnd(playhead, timelineDurationTicks);
    }
}
} // namespace Smm
