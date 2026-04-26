// Refulgent — single binary: global CLI flags, headless subcommands, or LibUI when appropriate.

#include "RefulgentCli.hxx"
#include "RefulgentGui.hxx"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

#ifndef REFULGENT_VERSION_STRING
#define REFULGENT_VERSION_STRING "1.0.0"
#endif

void PrintVersion() {
    std::cout << "Refulgent " REFULGENT_VERSION_STRING " — Solstice RELIC / bootstrap tool\n";
}

bool IsHeadlessSubcommand(std::string_view s) {
    static constexpr std::string_view k[] = {
        "info", "list", "stats", "verify", "diff", "export", "pack", "add", "remove", "extract", "merge", "bootstrap", "help",
    };
    for (const auto& x : k) {
        if (s == x) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    Refulgent::CliGlobalOptions g;
    std::vector<std::string> positional;
    bool help = false;
    bool wantVersion = false;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i] ? argv[i] : "";
        if (a[0] == '\0') {
            continue;
        }
        const std::string arg(a);
        if (arg == "-h" || arg == "-?" || arg == "--help") {
            help = true;
            continue;
        }
        if (arg == "-v" || arg == "--version") {
            wantVersion = true;
            continue;
        }
        if (arg == "--verbose") {
            g.Verbose = true;
            continue;
        }
        if (arg == "-q" || arg == "--quiet") {
            g.Quiet = true;
            continue;
        }
        if (arg == "--json") {
            g.JsonOutput = true;
            continue;
        }
        if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\nTry: \"" << (argv[0] ? argv[0] : "Refulgent") << " --help\"\n";
            return 2;
        }
        positional.push_back(arg);
    }

    if (help) {
        std::vector<std::string> av;
        av.reserve(2);
        av.push_back(argv[0] ? argv[0] : "Refulgent");
        av.push_back("help");
        std::vector<char*> ptrs;
        ptrs.push_back(av[0].data());
        ptrs.push_back(av[1].data());
        return Refulgent::RunRefulgentCli(2, ptrs.data(), g);
    }
    if (wantVersion) {
        PrintVersion();
        return 0;
    }

    if (positional.empty()) {
        return Refulgent::RunRefulgentGui(argc, argv, std::nullopt);
    }

    if (IsHeadlessSubcommand(positional[0])) {
        std::vector<std::string> av;
        av.push_back(argv[0] ? argv[0] : "Refulgent");
        av.insert(av.end(), positional.begin(), positional.end());
        std::vector<char*> ptrs;
        ptrs.reserve(av.size());
        for (auto& s : av) {
            ptrs.push_back(s.data());
        }
        return Refulgent::RunRefulgentCli(static_cast<int>(ptrs.size()), ptrs.data(), g);
    }

    if (positional.size() == 1) {
        std::error_code ec;
        const std::filesystem::path p(positional[0]);
        if (std::filesystem::is_regular_file(p, ec) || std::filesystem::exists(p, ec)) {
            return Refulgent::RunRefulgentGui(argc, argv, positional[0]);
        }
    }

    std::cerr << "Expected a subcommand (see --help) or a path to a .relic to open in the UI.\n";
    return 2;
}
