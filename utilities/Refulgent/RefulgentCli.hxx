#pragma once

#include <string>
#include <vector>

namespace Refulgent {

/// Options parsed before the first non-option argument (subcommand or path).
struct CliGlobalOptions {
    bool JsonOutput = false;
    bool Verbose = false;
    bool Quiet = false;
};

/// Headless RELIC / bootstrap commands. `argv` is the usual main vector; `argv[0]` is the program name,
/// `argv[1]` the first subcommand or `help`.
int RunRefulgentCli(int argc, char** argv, const CliGlobalOptions& opts);

} // namespace Refulgent
