#include "LibUI/Shell/DropFile.hxx"

namespace LibUI::Shell {

void CollectDropFilePathsFromEvent(const SDL_Event& e, std::vector<std::string>& out) {
    if (e.type == SDL_EVENT_DROP_FILE && e.drop.data) {
        out.emplace_back(e.drop.data);
    }
}

} // namespace LibUI::Shell
