#include "FileDialogs.hxx"

#include <cstring>
#include <utility>

namespace LibUI::FileDialogs {

namespace {

struct DialogUserData {
    std::function<void(std::optional<std::string>)> Callback;
    std::vector<FileFilter> Filters;
    std::vector<SDL_DialogFileFilter> SdlFilters;
};

void SDLCALL FileDialogCallback(void* userdata, const char* const* filelist, int filter) {
    (void)filter;
    auto* ud = static_cast<DialogUserData*>(userdata);
    if (!ud || !ud->Callback) {
        delete ud;
        return;
    }
    if (!filelist || !filelist[0]) {
        ud->Callback(std::nullopt);
    } else {
        ud->Callback(std::string(filelist[0]));
    }
    delete ud;
}

static void FillDefaultParallaxFilters(DialogUserData& ud) {
    ud.Filters = {FileFilter{"PARALLAX", "prlx"}, FileFilter{"All", "*"}};
}

static void BuildSdlFilters(DialogUserData& ud) {
    ud.SdlFilters.clear();
    ud.SdlFilters.reserve(ud.Filters.size());
    for (const auto& f : ud.Filters) {
        ud.SdlFilters.push_back(SDL_DialogFileFilter{f.Name.c_str(), f.Pattern.c_str()});
    }
}

} // namespace

void ShowOpenFile(SDL_Window* window, const char* title, std::function<void(std::optional<std::string>)> onResult,
    std::span<const FileFilter> filters) {
    (void)title;
    auto* ud = new DialogUserData;
    ud->Callback = std::move(onResult);
    if (filters.empty()) {
        FillDefaultParallaxFilters(*ud);
    } else {
        ud->Filters.assign(filters.begin(), filters.end());
    }
    BuildSdlFilters(*ud);
    SDL_ShowOpenFileDialog(FileDialogCallback, ud, window, ud->SdlFilters.data(), static_cast<int>(ud->SdlFilters.size()),
        nullptr, false);
}

void ShowSaveFile(SDL_Window* window, const char* title, std::function<void(std::optional<std::string>)> onResult,
    std::span<const FileFilter> filters) {
    (void)title;
    auto* ud = new DialogUserData;
    ud->Callback = std::move(onResult);
    if (filters.empty()) {
        FillDefaultParallaxFilters(*ud);
    } else {
        ud->Filters.assign(filters.begin(), filters.end());
    }
    BuildSdlFilters(*ud);
    SDL_ShowSaveFileDialog(FileDialogCallback, ud, window, ud->SdlFilters.data(), static_cast<int>(ud->SdlFilters.size()),
        nullptr);
}

} // namespace LibUI::FileDialogs
