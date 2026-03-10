#include "Bootstrap.hxx"
#include "../Debug.hxx"
#include <fstream>
#include <cstring>

namespace Solstice::Core::Relic {

std::filesystem::path GetDefaultBootstrapPath(const std::filesystem::path& basePath) {
    return basePath / "game.data.relic";
}

std::optional<BootstrapConfig> ParseBootstrap(const std::filesystem::path& path) {
    BootstrapConfig config;
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        return std::nullopt;
    }

    uint32_t magic = 0;
    f.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (!f || magic != RELIC_BOOTSTRAP_MAGIC) {
        return std::nullopt;
    }

    uint16_t version = 0;
    uint16_t reserved2 = 0;
    uint32_t entryCount = 0;
    f.read(reinterpret_cast<char*>(&version), sizeof(version));
    f.read(reinterpret_cast<char*>(&reserved2), sizeof(reserved2));
    f.read(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));
    if (!f || version != RELIC_FORMAT_VERSION) {
        return std::nullopt;
    }

    config.Entries.reserve(entryCount > 0 && entryCount < 65536u ? entryCount : 0u);
    for (uint32_t i = 0; i < entryCount && f; ++i) {
        BootstrapEntry entry;
        uint16_t pathLen = 0;
        f.read(reinterpret_cast<char*>(&pathLen), sizeof(pathLen));
        if (!f || pathLen > 4096u) {
            break;
        }
        entry.Path.resize(pathLen);
        if (pathLen > 0) {
            f.read(&entry.Path[0], pathLen);
        }
        f.read(reinterpret_cast<char*>(&entry.Priority), sizeof(entry.Priority));
        uint8_t hint = 0;
        f.read(reinterpret_cast<char*>(&hint), sizeof(hint));
        entry.Hint = static_cast<StreamingHint>(hint);
        f.read(reinterpret_cast<char*>(&entry.TagSet), sizeof(entry.TagSet));
        if (!f) {
            break;
        }
        config.Entries.push_back(std::move(entry));
    }

    if (!f) {
        return std::nullopt;
    }
    config.Valid = true;
    return config;
}

} // namespace Solstice::Core::Relic
