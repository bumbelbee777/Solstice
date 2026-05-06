#include "MazeGame.hxx"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] std::uint64_t ParseSeedArgv(int argc, char** argv) {
    constexpr std::uint64_t fallback = 42ULL;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.rfind("--seed=", 0) == 0 && a.size() > 7) {
            try {
                return std::stoull(a.substr(7));
            } catch (...) {
                return fallback;
            }
        }
        if ((a == "-s" || a == "--seed") && i + 1 < argc) {
            try {
                return std::stoull(argv[i + 1]);
            } catch (...) {
                return fallback;
            }
        }
    }
    if (const char* ev = std::getenv("MAZE_SEED"))
        try {
            return std::stoull(ev);
        } catch (...) {
        }
    return fallback;
}

} // namespace

int main(int argc, char** argv) {
    try {
        Solstice::MazeExp::MazeGame game(ParseSeedArgv(argc, argv));
        return game.Run();
    } catch (const std::exception& e) {
        std::cerr << "Maze fatal: " << e.what() << '\n';
        return 1;
    }
}
