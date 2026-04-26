// Refulgent — LibUI + ImGui (invoked from Main.cxx when not using headless subcommands).

#include "RefulgentGui.hxx"

#include "RelicOps.hxx"

#include <Core/Relic/Bootstrap.hxx>
#include <Core/Relic/PathTable.hxx>
#include <Core/Relic/Reader.hxx>
#include <Core/Relic/Types.hxx>
#include <Core/Relic/Unpack.hxx>

#include <SDL3/SDL_opengl.h>

#include "LibUI/Core/Core.hxx"
#include "LibUI/FileDialogs/FileDialogs.hxx"
#include "LibUI/Icons/Icons.hxx"
#include "LibUI/Shell/DropFile.hxx"
#include "LibUI/Shell/GlWindow.hxx"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using Solstice::Core::Relic::AssetTypeTag;
using Solstice::Core::Relic::GetCompressionType;
using Solstice::Core::Relic::GetDependencies;
using Solstice::Core::Relic::OpenRelic;
using Solstice::Core::Relic::ParseBootstrap;
using Solstice::Core::Relic::ParsePathTableBlob;
using Solstice::Core::Relic::UnpackRelic;

const LibUI::FileDialogs::FileFilter kRelicFilters[] = {
    {"RELIC archive", "relic"},
    {"All files", "*"},
};

/// Load Phosphor the same way as LibUI::Core when ``SOLSTICE_ENABLE_ICON_FONT`` is set, but always try the
/// bundled ``fonts/Phosphor.ttf`` next to the executable (no env var required for Refulgent).
static bool LoadRefulgentIconFontPack() {
    bool loaded = false;
    if (const char* iconEnv = std::getenv("SOLSTICE_ICON_FONT")) {
        if (iconEnv[0] != '\0' && std::filesystem::is_regular_file(iconEnv)) {
            loaded = LibUI::Icons::TryLoadIconFontPackFromFile(iconEnv, 16.f);
        }
    }
    if (!loaded) {
        if (const char* base = SDL_GetBasePath()) {
            try {
                const std::filesystem::path p = std::filesystem::path(base) / "fonts" / "Phosphor.ttf";
                if (std::filesystem::is_regular_file(p)) {
                    loaded = LibUI::Icons::TryLoadIconFontPackFromFile(p.string().c_str(), 16.f);
                }
            } catch (...) {
            }
        }
    }
    if (loaded) {
        LibUI::Icons::RefreshFontGpuTexture();
    }
    return loaded;
}

} // namespace

namespace Refulgent {

int RunRefulgentGui(int, char**, const std::optional<std::string>& initialRelic) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    LibUI::Shell::GlWindow glWindow;
    if (!LibUI::Shell::CreateUtilityGlWindow(
            glWindow, "Refulgent — RELIC archive manager", 1100, 720, SDL_WINDOW_RESIZABLE, 1)) {
        std::cerr << "CreateUtilityGlWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    SDL_Window* window = glWindow.window;

    if (!LibUI::Core::Initialize(window)) {
        std::cerr << "LibUI::Core::Initialize failed" << std::endl;
        LibUI::Shell::DestroyUtilityGlWindow(glWindow);
        SDL_Quit();
        return 1;
    }
    (void)LoadRefulgentIconFontPack();

    std::string currentPath;
    std::optional<Solstice::Core::Relic::RelicContainer> container;
    std::unordered_map<uint64_t, std::string> hashToPath;
    int selectedRow = -1;
    char exportDirBuf[1024] = "";
    char addFileBuf[1024] = "";
    char logicalPathBuf[512] = "";
    char filterBuf[256] = "";
    char hashCopyBuf[40] = "";
    std::string statusLine = "Open a .relic file, use File > Open, or drop files onto the window.";

    auto rowMatchesFilter = [&](const char* hbuf, const char* logPath) -> bool {
        if (filterBuf[0] == '\0') {
            return true;
        }
        std::string needle = filterBuf;
        for (char& c : needle) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        auto lower = [](std::string s) {
            for (char& c : s) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return s;
        };
        if (lower(std::string(hbuf)).find(needle) != std::string::npos) {
            return true;
        }
        if (logPath) {
            if (lower(std::string(logPath)).find(needle) != std::string::npos) {
                return true;
            }
        }
        return false;
    };

    auto manifestRowMatchesFilter = [&](int ri) -> bool {
        if (filterBuf[0] == '\0') {
            return true;
        }
        if (!container || ri < 0 || ri >= static_cast<int>(container->Manifest.size())) {
            return false;
        }
        const auto& e = container->Manifest[static_cast<size_t>(ri)];
        char hbuf[32];
        std::snprintf(hbuf, sizeof(hbuf), "%016llX", static_cast<unsigned long long>(e.AssetHash));
        const char* logStr = "—";
        auto pit = hashToPath.find(e.AssetHash);
        if (pit != hashToPath.end()) {
            logStr = pit->second.c_str();
        }
        return rowMatchesFilter(hbuf, logStr);
    };

    auto reloadFromPath = [&](const std::filesystem::path& p) {
        currentPath = p.string();
        container = OpenRelic(p);
        hashToPath.clear();
        selectedRow = -1;
        filterBuf[0] = '\0';
        hashCopyBuf[0] = '\0';
        if (!container) {
            if (auto b = ParseBootstrap(p)) {
                if (b->Valid) {
                    statusLine = "This path is a bootstrap manifest (not a container). Use: Refulgent bootstrap list \"" + p.string() + "\"";
                } else {
                    statusLine = "Failed to open as RELIC or bootstrap.";
                }
            } else {
                statusLine = "Failed to open RELIC.";
            }
            return;
        }
        if (!container->PathTableBlob.empty()) {
            std::vector<std::pair<std::string, uint64_t>> rows;
            if (ParsePathTableBlob(
                    std::span<const std::byte>(container->PathTableBlob.data(), container->PathTableBlob.size()), rows)) {
                for (const auto& [path, h] : rows) {
                    hashToPath[h] = path;
                }
            }
        }
        statusLine = "Loaded " + currentPath + " (" + std::to_string(container->Manifest.size()) + " entries).";
        LibUI::Core::RecentPathPush(currentPath.c_str());
    };

    if (initialRelic) {
        reloadFromPath(*initialRelic);
    }

    bool running = true;
    while (running) {
        std::vector<std::string> frameDropPaths;
        frameDropPaths.reserve(8);
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            LibUI::Shell::CollectDropFilePathsFromEvent(e, frameDropPaths);
            LibUI::Core::ProcessEvent(&e);
            if (e.type == SDL_EVENT_DROP_FILE && e.drop.data) {
                SDL_free((void*)e.drop.data);
            }
            if (e.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        for (const std::string& dropped : frameDropPaths) {
            std::error_code ec;
            const std::filesystem::path dp(dropped);
            if (std::filesystem::is_directory(dp, ec)) {
                const std::string s = dp.string();
                std::snprintf(exportDirBuf, sizeof(exportDirBuf), "%s", s.c_str());
                statusLine = "Export directory set from drop: " + s;
                continue;
            }
            if (std::filesystem::is_regular_file(dp, ec)) {
                reloadFromPath(dp);
            }
        }

        LibUI::Core::NewFrame();

        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);
        ImGui::Begin("RefulgentRoot", nullptr,
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Open, "Open RELIC...")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Open RELIC", [&](std::optional<std::string> path) {
                            if (path) {
                                reloadFromPath(*path);
                            }
                        },
                        kRelicFilters);
                }
                if (ImGui::BeginMenu("Open recent", LibUI::Core::RecentPathGetCount() > 0)) {
                    for (int ri = 0; ri < LibUI::Core::RecentPathGetCount(); ++ri) {
                        const char* rp = LibUI::Core::RecentPathGet(ri);
                        if (rp && ImGui::MenuItem(rp)) {
                            reloadFromPath(std::filesystem::path(rp));
                        }
                    }
                    ImGui::EndMenu();
                }
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Save, "Save (rewrite)", nullptr, false,
                        static_cast<bool>(container))) {
                    if (container) {
                        std::string err;
                        Refulgent::RelicOps::LoadError le;
                        auto inputs = Refulgent::RelicOps::LoadEntriesForRewrite(currentPath, &le);
                        if (!inputs) {
                            statusLine = le.Message;
                        } else {
                            if (Refulgent::RelicOps::WriteRelicFile(std::filesystem::path(currentPath), std::move(*inputs),
                                    Refulgent::RelicOps::OptionsFromHeader(container->Header), &err)) {
                                statusLine = "Saved " + currentPath;
                                reloadFromPath(std::filesystem::path(currentPath));
                            } else {
                                statusLine = err;
                            }
                        }
                    }
                }
                if (LibUI::Icons::MenuItemWithIcon(LibUI::Icons::Id::Export, "Save As…", nullptr, false,
                        static_cast<bool>(container))) {
                    if (container) {
                        LibUI::FileDialogs::ShowSaveFile(
                            window, "Save RELIC as", [&](std::optional<std::string> outPath) {
                                if (!outPath) {
                                    return;
                                }
                                std::string err;
                                Refulgent::RelicOps::LoadError le;
                                auto inputs = Refulgent::RelicOps::LoadEntriesForRewrite(currentPath, &le);
                                if (!inputs) {
                                    statusLine = le.Message;
                                    return;
                                }
                                if (Refulgent::RelicOps::WriteRelicFile(std::filesystem::path(*outPath), std::move(*inputs),
                                        Refulgent::RelicOps::OptionsFromHeader(container->Header), &err)) {
                                    currentPath = *outPath;
                                    LibUI::Core::RecentPathPush(currentPath.c_str());
                                    reloadFromPath(std::filesystem::path(currentPath));
                                    statusLine = "Saved a copy to " + currentPath;
                                } else {
                                    statusLine = err;
                                }
                            },
                            kRelicFilters);
                    }
                }
                if (ImGui::MenuItem("Quit")) {
                    running = false;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.75f, 0.86f, 1.f));
        ImGui::TextWrapped("Drop a .relic to open, or a folder to set the export directory.");
        ImGui::PopStyleColor();
        ImGui::Separator();

        if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Open, "Open…")) {
            LibUI::FileDialogs::ShowOpenFile(
                window, "Open RELIC", [&](std::optional<std::string> path) {
                    if (path) {
                        reloadFromPath(*path);
                    }
                },
                kRelicFilters);
        }
        ImGui::SameLine();
        if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Reload, "Refresh") && !currentPath.empty()) {
            reloadFromPath(std::filesystem::path(currentPath));
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Current file:");
        ImGui::SameLine();
        ImGui::TextUnformatted(currentPath.empty() ? "(none)" : currentPath.c_str());

        if (ImGui::CollapsingHeader("Summary##sum", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (container) {
                std::string err;
                if (auto st = Refulgent::RelicOps::GetArchiveStats(std::filesystem::path(currentPath), &err)) {
                    ImGui::Text("File size: %llu bytes | Entries: %zu | Compressed payload sum: %llu | Uncompressed sum: %llu",
                        static_cast<unsigned long long>(st->FileSizeBytes), st->EntryCount,
                        static_cast<unsigned long long>(st->TotalCompressedPayload),
                        static_cast<unsigned long long>(st->TotalUncompressed));
                    ImGui::Text("Path table: %s (%llu bytes) | Dependency table: %llu bytes", st->HasPathTable ? "yes" : "no",
                        static_cast<unsigned long long>(st->PathTableBytes), static_cast<unsigned long long>(st->DependencyTableBytes));
                } else {
                    ImGui::TextUnformatted(err.c_str());
                }
            } else {
                ImGui::TextDisabled("Open a RELIC to see aggregate sizes.");
            }
        }

        if (ImGui::CollapsingHeader("Unpack to folder##un", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::InputText("##exportdir", exportDirBuf, sizeof(exportDirBuf), ImGuiInputTextFlags_None);
            ImGui::SameLine();
            if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Export, "Export all##go") && container) {
                std::filesystem::path dir(exportDirBuf);
                if (dir.empty()) {
                    statusLine = "Set an export directory.";
                } else if (UnpackRelic(std::filesystem::path(currentPath), dir, true)) {
                    statusLine = "Exported to " + std::string(exportDirBuf);
                } else {
                    statusLine = "Export failed.";
                }
            }
            if (!container) {
                ImGui::TextDisabled("Open a RELIC first to unpack.");
            }
        }

        if (ImGui::CollapsingHeader("Add file to archive##ad")) {
            ImGui::TextUnformatted("Logical path is hashed (FNV-1a) per RelicOps; pick a file and a non-empty path.");
            ImGui::InputText("##addfile", addFileBuf, sizeof(addFileBuf));
            ImGui::InputText("Logical path", logicalPathBuf, sizeof(logicalPathBuf));
            if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Import, "Add to archive") && container) {
                std::filesystem::path fp(addFileBuf);
                std::string logical = Refulgent::RelicOps::NormalizeLogicalPath(logicalPathBuf);
                if (logical.empty() || !std::filesystem::is_regular_file(fp)) {
                    statusLine = "Need a file and a non-empty logical path.";
                } else {
                    std::vector<std::pair<std::filesystem::path, std::string>> b;
                    b.push_back({fp, logical});
                    std::string err;
                    auto merged = Refulgent::RelicOps::AddFilesToContainer(
                        std::filesystem::path(currentPath), b, Solstice::Core::Relic::CompressionType::Zstd, &err);
                    if (merged) {
                        if (Refulgent::RelicOps::WriteRelicFile(std::filesystem::path(currentPath), std::move(*merged),
                                Refulgent::RelicOps::OptionsFromHeader(container->Header), &err)) {
                            statusLine = "Added and saved.";
                            reloadFromPath(std::filesystem::path(currentPath));
                        } else {
                            statusLine = err;
                        }
                    } else {
                        statusLine = err;
                    }
                }
            }
        }

        if (ImGui::CollapsingHeader("Manifest##man", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextUnformatted("Case-insensitive filter (hash or logical path).");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                ImGui::SetTooltip("The Deps column is the number of listed dependency hashes for each manifest row.");
            }
            ImGui::InputText("##filter", filterBuf, sizeof(filterBuf));
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear filter")) {
                filterBuf[0] = '\0';
            }

            const float tableH = ImGui::GetContentRegionAvail().y - 100.0f;
            if (ImGui::BeginTable("entries", 8,
                ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY,
                ImVec2(0, std::max(120.0f, tableH)))) {
            ImGui::TableSetupColumn("Hash");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Comp");
            ImGui::TableSetupColumn("csize");
            ImGui::TableSetupColumn("usize");
            ImGui::TableSetupColumn("Cluster");
            ImGui::TableSetupColumn("Deps");
            ImGui::TableSetupColumn("Logical path");
            ImGui::TableHeadersRow();
            if (container) {
                for (int ri = 0; ri < static_cast<int>(container->Manifest.size()); ++ri) {
                    const auto& e = container->Manifest[static_cast<size_t>(ri)];
                    char hbuf[32];
                    std::snprintf(hbuf, sizeof(hbuf), "%016llX", static_cast<unsigned long long>(e.AssetHash));
                    const char* logStr = "—";
                    auto it = hashToPath.find(e.AssetHash);
                    if (it != hashToPath.end()) {
                        logStr = it->second.c_str();
                    }
                    if (!rowMatchesFilter(hbuf, logStr)) {
                        continue;
                    }
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Selectable(hbuf, selectedRow == ri)) {
                        selectedRow = ri;
                        std::snprintf(hashCopyBuf, sizeof(hashCopyBuf), "%s", hbuf);
                    }
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(
                        Refulgent::RelicOps::AssetTypeShortName(static_cast<AssetTypeTag>(e.AssetTypeTag)));
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(Refulgent::RelicOps::CompressionName(GetCompressionType(e.Flags)));
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%u", e.CompressedSize);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%u", e.UncompressedSize);
                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("%u", e.ClusterId);
                    ImGui::TableSetColumnIndex(6);
                    {
                        std::vector<Solstice::Core::Relic::AssetHash> deps;
                        GetDependencies(*container, e, deps);
                        ImGui::Text("%zu", deps.size());
                    }
                    ImGui::TableSetColumnIndex(7);
                    ImGui::TextUnformatted(it != hashToPath.end() ? it->second.c_str() : "—");
                }
            }
            ImGui::EndTable();
            }
            if (container && selectedRow >= 0 && !manifestRowMatchesFilter(selectedRow)) {
                selectedRow = -1;
                hashCopyBuf[0] = '\0';
            }
            if (selectedRow >= 0 && container && hashCopyBuf[0] != '\0') {
                ImGui::TextUnformatted("Selected hash:");
                ImGui::SameLine();
                ImGui::TextUnformatted(hashCopyBuf);
                ImGui::SameLine();
                if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Duplicate, "Copy")) {
                    ImGui::SetClipboardText(hashCopyBuf);
                }
            }
            if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Remove, "Remove selected") && container
                && selectedRow >= 0 && selectedRow < static_cast<int>(container->Manifest.size())) {
                const uint64_t h = container->Manifest[static_cast<size_t>(selectedRow)].AssetHash;
                std::string err;
                auto kept = Refulgent::RelicOps::RemoveHashes(std::filesystem::path(currentPath), {h}, &err);
                if (kept && !kept->empty()) {
                    if (Refulgent::RelicOps::WriteRelicFile(std::filesystem::path(currentPath), std::move(*kept),
                            Refulgent::RelicOps::OptionsFromHeader(container->Header), &err)) {
                        statusLine = "Removed entry.";
                        selectedRow = -1;
                        reloadFromPath(std::filesystem::path(currentPath));
                    } else {
                        statusLine = err;
                    }
                } else {
                    statusLine = err.empty() ? "Remove failed." : err;
                }
            }
        }

        ImGui::Separator();
        ImGui::BeginChild("##status", ImVec2(0, 52.0f), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f, 0.9f, 0.94f, 1.f));
        ImGui::TextWrapped("%s", statusLine.c_str());
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::End();

        glClearColor(0.1f, 0.1f, 0.12f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        LibUI::Core::Render();
        SDL_GL_SwapWindow(window);
    }

    LibUI::Core::Shutdown();
    LibUI::Shell::DestroyUtilityGlWindow(glWindow);
    SDL_Quit();
    return 0;
}

} // namespace Refulgent
