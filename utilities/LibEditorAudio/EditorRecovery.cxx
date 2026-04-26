#include <Solstice/EditorAudio/EditorRecovery.hxx>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <mutex>
#include <random>
#include <span>
#include <sstream>

namespace {

constexpr char kMagic[4] = {'S', 'L', 'R', 'C'};
constexpr std::uint32_t kVersion = 1;

void WriteLeU32(std::vector<std::byte>& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        b.push_back(static_cast<std::byte>((v >> (8 * i)) & 0xFFu));
    }
}

void WriteLeU64(std::vector<std::byte>& b, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        b.push_back(static_cast<std::byte>((v >> (8 * i)) & 0xFFu));
    }
}

bool ReadLeU32(std::span<const std::byte> s, size_t& off, std::uint32_t& out) {
    if (off + 4 > s.size()) {
        return false;
    }
    out = 0;
    for (int i = 0; i < 4; ++i) {
        out |= (std::uint32_t) static_cast<std::uint8_t>(s[off + (size_t)i]) << (8 * i);
    }
    off += 4;
    return true;
}

bool ReadLeU64(std::span<const std::byte> s, size_t& off, std::uint64_t& out) {
    if (off + 8 > s.size()) {
        return false;
    }
    out = 0;
    for (int i = 0; i < 8; ++i) {
        out |= (std::uint64_t) static_cast<std::uint8_t>(s[off + (size_t)i]) << (8 * i);
    }
    off += 8;
    return true;
}

[[nodiscard]] std::uint64_t nowUnixUtc() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

} // namespace

namespace Solstice::EditorAudio::FileRecovery {

std::filesystem::path RecoveryDir(const char* sdlBasePathUtf8OrNull, std::string_view appSlug) {
    const std::filesystem::path base = sdlBasePathUtf8OrNull ? std::filesystem::path(sdlBasePathUtf8OrNull) : std::filesystem::path(".");
    return base / ".solstice_recovery" / appSlug;
}

bool WriteSnapshot(
    const std::filesystem::path& recoveryDir, std::string_view fileNamePrefix, std::span<const std::byte> payload, std::string* outError) {
    if (outError) {
        outError->clear();
    }
    try {
        std::error_code ec;
        std::filesystem::create_directories(recoveryDir, ec);
        if (ec) {
            if (outError) {
                *outError = "create_directories: " + ec.message();
            }
            return false;
        }
    } catch (const std::exception& e) {
        if (outError) {
            *outError = e.what();
        }
        return false;
    }
    const std::uint64_t t = nowUnixUtc();
    static std::mutex g_TagMtx;
    static std::mt19937_64 gRng{std::random_device{}()};
    std::uint64_t tag = 0;
    {
        std::lock_guard<std::mutex> lock(g_TagMtx);
        tag = gRng();
    }
    std::ostringstream oss;
    oss << fileNamePrefix << "_" << t << "_" << std::hex << tag << ".srec";
    const std::filesystem::path outPath = recoveryDir / oss.str();

    std::vector<std::byte> fileBytes;
    fileBytes.reserve(4u + 4u + 8u + 8u + payload.size());
    for (int i = 0; i < 4; ++i) {
        fileBytes.push_back(std::byte{static_cast<unsigned char>(kMagic[i])});
    }
    WriteLeU32(fileBytes, kVersion);
    WriteLeU64(fileBytes, t);
    WriteLeU64(fileBytes, static_cast<std::uint64_t>(payload.size()));
    fileBytes.insert(fileBytes.end(), payload.begin(), payload.end());

    try {
        std::ofstream f(outPath, std::ios::binary | std::ios::trunc);
        if (!f) {
            if (outError) {
                *outError = "open failed: " + outPath.string();
            }
            return false;
        }
        f.write(reinterpret_cast<const char*>(fileBytes.data()), static_cast<std::streamsize>(fileBytes.size()));
        if (!f) {
            if (outError) {
                *outError = "write failed: " + outPath.string();
            }
            return false;
        }
    } catch (const std::exception& e) {
        if (outError) {
            *outError = e.what();
        }
        return false;
    }
    return true;
}

std::vector<Entry> List(const std::filesystem::path& recoveryDir, std::string_view fileNamePrefix) {
    std::vector<Entry> out;
    std::error_code ec;
    if (!std::filesystem::is_directory(recoveryDir, ec)) {
        return out;
    }
    const std::string pfx{fileNamePrefix};
    for (const auto& ent : std::filesystem::directory_iterator(recoveryDir, ec)) {
        if (!ent.is_regular_file()) {
            continue;
        }
        const auto name = ent.path().filename().string();
        if (name.size() < 6 || name.compare(name.size() - 4, 4, ".srec") != 0) {
            continue;
        }
        if (name.size() < pfx.size() + 2 || name.compare(0, pfx.size(), pfx) != 0) {
            continue;
        }
        std::ifstream f(ent.path(), std::ios::binary);
        if (!f) {
            continue;
        }
        std::vector<char> buf(
            (std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (buf.size() < 4 + 4 + 8 + 8) {
            continue;
        }
        if (std::memcmp(buf.data(), kMagic, 4) != 0) {
            continue;
        }
        const std::span<const std::byte> sp(reinterpret_cast<const std::byte*>(buf.data()), buf.size());
        size_t o = 4;
        std::uint32_t ver = 0;
        std::uint64_t unixT = 0;
        std::uint64_t payLen = 0;
        if (!ReadLeU32(sp, o, ver) || ver != kVersion) {
            continue;
        }
        if (!ReadLeU64(sp, o, unixT) || !ReadLeU64(sp, o, payLen) || o + payLen > sp.size()) {
            continue;
        }
        out.push_back(Entry{ent.path(), unixT, static_cast<size_t>(payLen)});
    }
    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) { return a.unixTimeUtc < b.unixTimeUtc; });
    return out;
}

bool ReadFile(const std::filesystem::path& file, std::vector<std::byte>& out, std::string* outError) {
    out.clear();
    if (outError) {
        outError->clear();
    }
    std::ifstream f(file, std::ios::binary);
    if (!f) {
        if (outError) {
            *outError = "open failed: " + file.string();
        }
        return false;
    }
    std::vector<char> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (buf.size() < 4 + 4 + 8 + 8) {
        if (outError) {
            *outError = "truncated file";
        }
        return false;
    }
    if (std::memcmp(buf.data(), kMagic, 4) != 0) {
        if (outError) {
            *outError = "bad magic";
        }
        return false;
    }
    const std::span<const std::byte> sp(reinterpret_cast<const std::byte*>(buf.data()), buf.size());
    size_t o = 4;
    std::uint32_t ver = 0;
    std::uint64_t unixT = 0;
    std::uint64_t payLen = 0;
    if (!ReadLeU32(sp, o, ver) || ver != kVersion) {
        if (outError) {
            *outError = "version mismatch";
        }
        return false;
    }
    if (!ReadLeU64(sp, o, unixT) || !ReadLeU64(sp, o, payLen) || o + payLen > sp.size()) {
        if (outError) {
            *outError = "header corrupt";
        }
        return false;
    }
    out.resize(static_cast<size_t>(payLen));
    std::memcpy(out.data(), buf.data() + o, static_cast<size_t>(payLen));
    (void)unixT;
    return true;
}

bool ReadLatest(
    const std::filesystem::path& recoveryDir, std::string_view fileNamePrefix, std::vector<std::byte>& out, std::string* outError) {
    const std::vector<Entry> e = List(recoveryDir, fileNamePrefix);
    if (e.empty()) {
        if (outError) {
            *outError = "no recovery files";
        }
        return false;
    }
    return ReadFile(e.back().path, out, outError);
}

void ClearMatchingPrefix(const std::filesystem::path& recoveryDir, std::string_view fileNamePrefix) {
    for (const auto& e : List(recoveryDir, fileNamePrefix)) {
        std::error_code ec;
        std::filesystem::remove(e.path, ec);
    }
}

} // namespace Solstice::EditorAudio::FileRecovery
