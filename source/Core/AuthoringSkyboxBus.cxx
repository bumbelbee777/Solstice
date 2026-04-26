#include <Core/AuthoringSkyboxBus.hxx>

#include <mutex>

namespace Solstice::Core {
namespace {
std::mutex g_Mutex;
AuthoringSkyboxState g_State{};
uint64_t g_NextRevision = 1;
} // namespace

void PublishAuthoringSkyboxState(const AuthoringSkyboxState& state) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_State = state;
    g_State.Revision = g_NextRevision++;
}

AuthoringSkyboxState GetAuthoringSkyboxState() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    return g_State;
}

} // namespace Solstice::Core
