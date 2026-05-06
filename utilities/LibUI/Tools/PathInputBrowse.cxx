#include "LibUI/Tools/PathInputBrowse.hxx"

#include "LibUI/FileDialogs/FileDialogs.hxx"
#include "LibUI/Widgets/Widgets.hxx"

#include <algorithm>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace LibUI::Tools {

void InputPathOpenBrowseRowHint(float trailingReservePx, const char* inputId, const char* hint, char* buf, size_t bufSize,
    SDL_Window* window, const char* dialogTitle, const char* browseButtonLabel,
    std::span<const LibUI::FileDialogs::FileFilter> filters,
    const std::function<std::string(const std::string&)>& mapSelectedPath) {
    if (!inputId || !buf || bufSize == 0 || !window || !dialogTitle || !browseButtonLabel) {
        return;
    }
    const float avail = LibUI::Widgets::GetContentRegionAvail().x;
    LibUI::Widgets::SetNextItemWidth(std::max(1.0f, avail - trailingReservePx));
    LibUI::Widgets::InputTextWithHint(inputId, hint ? hint : "", buf, bufSize);
    LibUI::Widgets::SameLine();
    if (LibUI::Widgets::Button(browseButtonLabel)) {
        LibUI::FileDialogs::ShowOpenFile(
            window, dialogTitle,
            [buf, bufSize, mapSelectedPath](std::optional<std::string> path) {
                if (path) {
                    const std::string remapped = mapSelectedPath ? mapSelectedPath(*path) : *path;
                    std::strncpy(buf, remapped.c_str(), bufSize - 1);
                    buf[bufSize - 1] = '\0';
                }
            },
            filters);
    }
}

bool InputPathOpenBrowseRowHintString(float trailingReservePx, const char* inputId, const char* hint, std::string& value,
    size_t editBufferSize, SDL_Window* window, const char* dialogTitle, const char* browseButtonLabel,
    std::span<const LibUI::FileDialogs::FileFilter> filters,
    const std::function<std::string(const std::string&)>& mapSelectedPath, const std::function<void()>& onValueMutated) {
    if (!inputId || editBufferSize < 2 || !window || !dialogTitle || !browseButtonLabel) {
        return false;
    }

    std::vector<char> editBuffer(std::max(editBufferSize, value.size() + 1), '\0');
    std::strncpy(editBuffer.data(), value.c_str(), editBuffer.size() - 1);

    const float avail = LibUI::Widgets::GetContentRegionAvail().x;
    LibUI::Widgets::SetNextItemWidth(std::max(1.0f, avail - trailingReservePx));
    bool changed = false;
    if (LibUI::Widgets::InputTextWithHint(inputId, hint ? hint : "", editBuffer.data(), editBuffer.size())) {
        const std::string nextValue(editBuffer.data());
        if (nextValue != value) {
            if (onValueMutated) {
                onValueMutated();
            }
            value = nextValue;
            changed = true;
        }
    }

    LibUI::Widgets::SameLine();
    if (LibUI::Widgets::Button(browseButtonLabel)) {
        LibUI::FileDialogs::ShowOpenFile(
            window, dialogTitle,
            [&value, mapSelectedPath, onValueMutated](std::optional<std::string> path) {
                if (!path || path->empty()) {
                    return;
                }
                const std::string remapped = mapSelectedPath ? mapSelectedPath(*path) : *path;
                if (remapped == value) {
                    return;
                }
                if (onValueMutated) {
                    onValueMutated();
                }
                value = remapped;
            },
            filters);
    }
    return changed;
}

} // namespace LibUI::Tools
