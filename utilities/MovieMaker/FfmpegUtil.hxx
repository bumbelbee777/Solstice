#pragma once

#include <string>

struct SolsticeFfmpegRunResult {
    int ExitCode = -1;
    std::string Output;
};

/// Runs `executable` with `arguments` (not including argv[0]); captures combined stdout/stderr.
SolsticeFfmpegRunResult SolsticeRunProcessCapture(const std::string& executable, const std::string& arguments);
