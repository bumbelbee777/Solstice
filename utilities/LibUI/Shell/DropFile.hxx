#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace LibUI::Shell {

/// If `e.type` is `SDL_EVENT_DROP_FILE` and `e.drop.data` is non-null, appends a UTF-8 copy of the path.
/// The caller should ``SDL_free`` the event's `drop.data` after ``SDL_PollEvent`` / ``LibUI::Core::ProcessEvent`` for
/// that event (SDL3 allocates the path string for drop-file events).
void CollectDropFilePathsFromEvent(const SDL_Event& e, std::vector<std::string>& out);

} // namespace LibUI::Shell
