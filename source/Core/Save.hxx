#pragma once

#include "../Solstice.hxx"
#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <memory>

namespace Solstice::Core {

// Save data version
static constexpr uint32_t SAVE_VERSION = 1;
static constexpr uint32_t SAVE_MAGIC = 0x534F4C53; // "SOLS"

// Save data header
struct SaveHeader {
    uint32_t Magic{SAVE_MAGIC};
    uint32_t Version{SAVE_VERSION};
    uint64_t Timestamp{0};
    uint32_t Checksum{0};
    uint32_t DataSize{0};
};

// Save data structure
struct SaveData {
    SaveHeader Header;
    std::vector<uint8_t> Data;

    // Serialization helpers
    bool Serialize(std::ostream& Stream) const;
    bool Deserialize(std::istream& Stream);

    // Validation
    bool Validate() const;
    uint32_t CalculateChecksum() const;
};

// Save manager
class SOLSTICE_API SaveManager {
public:
    SaveManager();
    ~SaveManager() = default;

    // Save game state
    bool Save(const std::string& FilePath, const SaveData& Data);

    // Load game state
    bool Load(const std::string& FilePath, SaveData& OutData);

    // Validate save file
    bool Validate(const std::string& FilePath);

    // Attempt to repair corrupted save
    bool Repair(const std::string& FilePath);

    // Create backup
    bool CreateBackup(const std::string& FilePath);

    // Restore from backup
    bool RestoreBackup(const std::string& FilePath);

    // Get save slots
    std::vector<std::string> GetSaveSlots() const;

    // Delete save
    bool DeleteSave(const std::string& FilePath);

    // CRC32 checksum calculation (public for SaveData to use)
    static uint32_t CalculateCRC32(const uint8_t* Data, size_t Size);

private:
    std::string m_SaveDirectory{"saves"};
    std::string m_BackupDirectory{"saves/backups"};

    // File operations
    bool FileExists(const std::string& FilePath) const;
    bool CreateDirectory(const std::string& Path) const;
};

} // namespace Solstice::Core
