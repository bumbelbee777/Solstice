#pragma once

#include <optional>
#include <string>

namespace Refulgent {

/// Interactive RELIC manager (LibUI + ImGui). `initialRelic` preloads a file if present.
int RunRefulgentGui(int argc, char** argv, const std::optional<std::string>& initialRelic);

} // namespace Refulgent
