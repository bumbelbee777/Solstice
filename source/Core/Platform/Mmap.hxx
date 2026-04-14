#pragma once

#include "Solstice.hxx"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>

namespace Solstice::Core {

// Read-only file-backed memory mapping for sequential access (e.g. RELIC data blobs).
// Platform: Windows (CreateFileMapping / MapViewOfFile), POSIX (open + mmap).
class SOLSTICE_API MmapFile {
public:
    MmapFile() = default;
    ~MmapFile();

    MmapFile(const MmapFile&) = delete;
    MmapFile& operator=(const MmapFile&) = delete;
    MmapFile(MmapFile&& other) noexcept;
    MmapFile& operator=(MmapFile&& other) noexcept;

    // Open file and map region [offset, offset + size). If size is 0, map from offset to end of file.
    bool Open(const std::filesystem::path& path, uint64_t offset = 0, uint64_t size = 0);

    void Close();

    bool IsOpen() const { return m_Data != nullptr; }

    // Pointer to mapped region. Valid until Close() or destruction.
    const std::byte* Data() const { return m_Data; }
    std::byte* Data() { return m_Data; }

    size_t Size() const { return m_Size; }

    // View of a subregion. No copy; caller must not use after Close().
    std::span<const std::byte> Read(uint64_t offset, size_t length) const;

private:
    std::byte* m_Data = nullptr;
    size_t m_Size = 0;

#if defined(_WIN32) || defined(_WIN64)
    void* m_FileHandle = nullptr;
    void* m_MappingHandle = nullptr;
#else
    int m_Fd = -1;
#endif
};

} // namespace Solstice::Core
