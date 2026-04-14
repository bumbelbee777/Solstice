#include "FfmpegUtil.hxx"

#include <cstdio>
#include <memory>
#include <string>

#ifdef _WIN32
#include <stdlib.h>
#else
#include <stdlib.h>
#endif

SolsticeFfmpegRunResult SolsticeRunProcessCapture(const std::string& executable, const std::string& arguments) {
    SolsticeFfmpegRunResult r;
    std::string cmd = "\"" + executable + "\" " + arguments;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "rb");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) {
        r.Output = "Failed to start process.\n";
        return r;
    }
    char buf[1024];
    while (fgets(buf, sizeof(buf), pipe)) {
        r.Output += buf;
    }
#ifdef _WIN32
    r.ExitCode = _pclose(pipe);
#else
    r.ExitCode = pclose(pipe);
#endif
    if (r.ExitCode < 0) {
        r.ExitCode = -1;
    }
#ifdef _WIN32
    // _pclose returns process exit code in high bits on some MSVC; normalize
    if (r.ExitCode != -1 && r.ExitCode > 255) {
        r.ExitCode = (r.ExitCode >> 8) & 0xFF;
    }
#endif
    return r;
}
