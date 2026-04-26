#include "Bootstrap.hxx"
#include "Core/Debug/Debug.hxx"
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

bool WriteBootstrap(const std::filesystem::path& path, const std::vector<BootstrapEntry>& entries) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        return false;
    }
    const uint32_t magic = RELIC_BOOTSTRAP_MAGIC;
    const uint16_t version = RELIC_FORMAT_VERSION;
    const uint16_t reserved = 0;
    const uint32_t entryCount = static_cast<uint32_t>(entries.size());
    f.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    f.write(reinterpret_cast<const char*>(&version), sizeof(version));
    f.write(reinterpret_cast<const char*>(&reserved), sizeof(reserved));
    f.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));
    if (!f) {
        return false;
    }
    for (const auto& e : entries) {
        if (e.Path.size() > 4096u) {
            return false;
        }
        const uint16_t pathLen = static_cast<uint16_t>(e.Path.size());
        f.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
        if (pathLen > 0) {
            f.write(e.Path.data(), pathLen);
        }
        f.write(reinterpret_cast<const char*>(&e.Priority), sizeof(e.Priority));
        const uint8_t hint = static_cast<uint8_t>(e.Hint);
        f.write(reinterpret_cast<const char*>(&hint), sizeof(hint));
        f.write(reinterpret_cast<const char*>(&e.TagSet), sizeof(e.TagSet));
        if (!f) {
            return false;
        }
    }
    return static_cast<bool>(f);
}

} // namespace Solstice::Core::Relic
