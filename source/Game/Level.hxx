#pragma once

#include <vector>
#include <string>

namespace Solstice::Game {
struct Level {
    std::string Name;
};

class LevelManager {
public:
    LevelManager();
    ~LevelManager();

    void LoadLevel(const std::string& LevelName);
    void SaveLevel(const std::string& LevelName);
};
}
