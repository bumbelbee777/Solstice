#include "Mmap.hxx"
#include <Core/Debug/Debug.hxx>
#include <cstring>
#include <algorithm>

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace Solstice::Core {

#if defined(_WIN32) || defined(_WIN64)

MmapFile::~MmapFile() {
    Close();
}

MmapFile::MmapFile(MmapFile&& other) noexcept
    : m_Data(other.m_Data)
    , m_Size(other.m_Size)
    , m_FileHandle(other.m_FileHandle)
    , m_MappingHandle(other.m_MappingHandle) {
    other.m_Data = nullptr;
    other.m_Size = 0;
    other.m_FileHandle = nullptr;
    other.m_MappingHandle = nullptr;
}

MmapFile& MmapFile::operator=(MmapFile&& other) noexcept {
    if (this != &other) {
        Close();
        m_Data = other.m_Data;
        m_Size = other.m_Size;
        m_FileHandle = other.m_FileHandle;
        m_MappingHandle = other.m_MappingHandle;
        other.m_Data = nullptr;
        other.m_Size = 0;
        other.m_FileHandle = nullptr;
        other.m_MappingHandle = nullptr;
    }
    return *this;
}

bool MmapFile::Open(const std::filesystem::path& path, uint64_t offset, uint64_t size) {
    Close();
    std::wstring pathW = path.native();
    if (path.native().size() == 0) {
        return false;
    }
    HANDLE hFile = CreateFileW(pathW.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER fileSizeLi;
    if (!GetFileSizeEx(hFile, &fileSizeLi)) {
        CloseHandle(hFile);
        return false;
    }
    const uint64_t fileSize = static_cast<uint64_t>(fileSizeLi.QuadPart);
    if (offset >= fileSize) {
        CloseHandle(hFile);
        return false;
    }
    const uint64_t mapSize = (size == 0) ? (fileSize - offset) : size;
    if (offset + mapSize > fileSize) {
        CloseHandle(hFile);
        return false;
    }
    HANDLE hMap = CreateFileMappingW(hFile, nullptr, PAGE_READONLY,
                                    static_cast<DWORD>(mapSize >> 32), static_cast<DWORD>(mapSize), nullptr);
    if (!hMap) {
        CloseHandle(hFile);
        return false;
    }
    void* view = MapViewOfFile(hMap, FILE_MAP_READ, static_cast<DWORD>(offset >> 32),
                               static_cast<DWORD>(offset), static_cast<SIZE_T>(mapSize));
    CloseHandle(hFile);
    if (!view) {
        CloseHandle(hMap);
        return false;
    }
    m_Data = static_cast<std::byte*>(view);
    m_Size = static_cast<size_t>(mapSize);
    m_FileHandle = nullptr;
    m_MappingHandle = hMap;
    return true;
}

void MmapFile::Close() {
    if (m_Data) {
        UnmapViewOfFile(m_Data);
        m_Data = nullptr;
    }
    m_Size = 0;
    if (m_MappingHandle) {
        CloseHandle(static_cast<HANDLE>(m_MappingHandle));
        m_MappingHandle = nullptr;
    }
    m_FileHandle = nullptr;
}

#else

MmapFile::~MmapFile() {
    Close();
}

MmapFile::MmapFile(MmapFile&& other) noexcept
    : m_Data(other.m_Data)
    , m_Size(other.m_Size)
    , m_Fd(other.m_Fd) {
    other.m_Data = nullptr;
    other.m_Size = 0;
    other.m_Fd = -1;
}

MmapFile& MmapFile::operator=(MmapFile&& other) noexcept {
    if (this != &other) {
        Close();
        m_Data = other.m_Data;
        m_Size = other.m_Size;
        m_Fd = other.m_Fd;
        other.m_Data = nullptr;
        other.m_Size = 0;
        other.m_Fd = -1;
    }
    return *this;
}

bool MmapFile::Open(const std::filesystem::path& path, uint64_t offset, uint64_t size) {
    Close();
    int fd = open(path.string().c_str(), O_RDONLY);
    if (fd < 0) {
        return false;
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
        ::close(fd);
        return false;
    }
    const uint64_t fileSize = static_cast<uint64_t>(st.st_size);
    if (offset >= fileSize) {
        ::close(fd);
        return false;
    }
    const uint64_t mapSize = (size == 0) ? (fileSize - offset) : size;
    if (offset + mapSize > fileSize) {
        ::close(fd);
        return false;
    }
    void* view = mmap(nullptr, static_cast<size_t>(mapSize), PROT_READ, MAP_PRIVATE,
                      fd, static_cast<off_t>(offset));
    if (view == MAP_FAILED) {
        ::close(fd);
        return false;
    }
    m_Data = static_cast<std::byte*>(view);
    m_Size = static_cast<size_t>(mapSize);
    m_Fd = fd;
    return true;
}

void MmapFile::Close() {
    if (m_Data && m_Size > 0) {
        munmap(m_Data, m_Size);
        m_Data = nullptr;
        m_Size = 0;
    }
    if (m_Fd >= 0) {
        ::close(m_Fd);
        m_Fd = -1;
    }
}

#endif

std::span<const std::byte> MmapFile::Read(uint64_t offset, size_t length) const {
    if (offset >= m_Size || length == 0) {
        return {};
    }
    size_t available = static_cast<size_t>(m_Size - offset);
    size_t n = std::min(length, available);
    return std::span<const std::byte>(m_Data + offset, n);
}

} // namespace Solstice::Core
