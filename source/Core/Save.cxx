#include "Save.hxx"
#include "Debug.hxx"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <chrono>

namespace Solstice::Core {

// CRC32 lookup table (generated)
static const uint32_t CRC32_TABLE[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

SaveManager::SaveManager() {
    // Create save directories if they don't exist
    try {
        if (!std::filesystem::exists(m_SaveDirectory)) {
            std::filesystem::create_directories(m_SaveDirectory);
        }
        if (!std::filesystem::exists(m_BackupDirectory)) {
            std::filesystem::create_directories(m_BackupDirectory);
        }
    } catch (const std::exception& e) {
        SIMPLE_LOG("SaveManager: Failed to create directories: " + std::string(e.what()));
    }
}

bool SaveData::Serialize(std::ostream& Stream) const {
    try {
        // Write header
        Stream.write(reinterpret_cast<const char*>(&Header), sizeof(Header));

        // Write data
        if (!Data.empty()) {
            Stream.write(reinterpret_cast<const char*>(Data.data()), Data.size());
        }

        return Stream.good();
    } catch (const std::exception& e) {
        SIMPLE_LOG("SaveData: Serialization failed: " + std::string(e.what()));
        return false;
    }
}

bool SaveData::Deserialize(std::istream& Stream) {
    try {
        // Read header
        Stream.read(reinterpret_cast<char*>(&Header), sizeof(Header));

        if (Header.Magic != SAVE_MAGIC) {
            SIMPLE_LOG("SaveData: Invalid magic number");
            return false;
        }

        if (Header.Version != SAVE_VERSION) {
            SIMPLE_LOG("SaveData: Version mismatch. Expected " + std::to_string(SAVE_VERSION) +
                      ", got " + std::to_string(Header.Version));
            // Could implement version migration here
        }

        // Read data
        Data.resize(Header.DataSize);
        if (Header.DataSize > 0) {
            Stream.read(reinterpret_cast<char*>(Data.data()), Header.DataSize);
        }

        return Stream.good();
    } catch (const std::exception& e) {
        SIMPLE_LOG("SaveData: Deserialization failed: " + std::string(e.what()));
        return false;
    }
}

bool SaveData::Validate() const {
    // Check magic
    if (Header.Magic != SAVE_MAGIC) {
        return false;
    }

    // Check version
    if (Header.Version != SAVE_VERSION) {
        return false;
    }

    // Check checksum
    uint32_t calculatedChecksum = CalculateChecksum();
    if (calculatedChecksum != Header.Checksum) {
        SIMPLE_LOG("SaveData: Checksum mismatch. Expected " + std::to_string(Header.Checksum) +
                  ", got " + std::to_string(calculatedChecksum));
        return false;
    }

    // Check data size
    if (Data.size() != Header.DataSize) {
        return false;
    }

    return true;
}

uint32_t SaveData::CalculateChecksum() const {
    if (Data.empty()) {
        return 0;
    }
    return SaveManager::CalculateCRC32(Data.data(), Data.size());
}

bool SaveManager::Save(const std::string& FilePath, const SaveData& Data) {
    try {
        // Create backup before saving
        if (FileExists(FilePath)) {
            CreateBackup(FilePath);
        }

        // Prepare save data
        SaveData saveData = Data;
        saveData.Header.DataSize = static_cast<uint32_t>(saveData.Data.size());
        saveData.Header.Timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        saveData.Header.Checksum = saveData.CalculateChecksum();

        // Write to file
        std::ofstream file(FilePath, std::ios::binary);
        if (!file.is_open()) {
            SIMPLE_LOG("SaveManager: Failed to open file for writing: " + FilePath);
            return false;
        }

        if (!saveData.Serialize(file)) {
            SIMPLE_LOG("SaveManager: Failed to serialize save data");
            return false;
        }

        file.close();
        SIMPLE_LOG("SaveManager: Successfully saved to: " + FilePath);
        return true;
    } catch (const std::exception& e) {
        SIMPLE_LOG("SaveManager: Save failed: " + std::string(e.what()));
        return false;
    }
}

bool SaveManager::Load(const std::string& FilePath, SaveData& OutData) {
    try {
        std::ifstream file(FilePath, std::ios::binary);
        if (!file.is_open()) {
            SIMPLE_LOG("SaveManager: Failed to open file for reading: " + FilePath);
            return false;
        }

        if (!OutData.Deserialize(file)) {
            SIMPLE_LOG("SaveManager: Failed to deserialize save data");
            return false;
        }

        file.close();

        // Validate
        if (!OutData.Validate()) {
            SIMPLE_LOG("SaveManager: Save data validation failed, attempting repair...");
            if (!Repair(FilePath)) {
                SIMPLE_LOG("SaveManager: Repair failed");
                return false;
            }
            // Try loading again after repair
            return Load(FilePath, OutData);
        }

        SIMPLE_LOG("SaveManager: Successfully loaded from: " + FilePath);
        return true;
    } catch (const std::exception& e) {
        SIMPLE_LOG("SaveManager: Load failed: " + std::string(e.what()));
        return false;
    }
}

bool SaveManager::Validate(const std::string& FilePath) {
    SaveData data;
    if (!Load(FilePath, data)) {
        return false;
    }
    return data.Validate();
}

bool SaveManager::Repair(const std::string& FilePath) {
    try {
        SIMPLE_LOG("SaveManager: Attempting to repair: " + FilePath);

        // Try to restore from backup
        if (RestoreBackup(FilePath)) {
            SIMPLE_LOG("SaveManager: Restored from backup");
            return true;
        }

        // Try to read partial data
        std::ifstream file(FilePath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        SaveData data;
        if (data.Deserialize(file)) {
            // Recalculate checksum and try to fix
            data.Header.Checksum = data.CalculateChecksum();
            file.close();

            // Try to save repaired data
            return Save(FilePath, data);
        }

        return false;
    } catch (const std::exception& e) {
        SIMPLE_LOG("SaveManager: Repair failed: " + std::string(e.what()));
        return false;
    }
}

bool SaveManager::CreateBackup(const std::string& FilePath) {
    try {
        if (!FileExists(FilePath)) {
            return false;
        }

        std::filesystem::path path(FilePath);
        std::string backupPath = m_BackupDirectory + "/" + path.filename().string() + ".backup";

        std::filesystem::copy_file(FilePath, backupPath, std::filesystem::copy_options::overwrite_existing);
        SIMPLE_LOG("SaveManager: Created backup: " + backupPath);
        return true;
    } catch (const std::exception& e) {
        SIMPLE_LOG("SaveManager: Backup creation failed: " + std::string(e.what()));
        return false;
    }
}

bool SaveManager::RestoreBackup(const std::string& FilePath) {
    try {
        std::filesystem::path path(FilePath);
        std::string backupPath = m_BackupDirectory + "/" + path.filename().string() + ".backup";

        if (!FileExists(backupPath)) {
            return false;
        }

        std::filesystem::copy_file(backupPath, FilePath, std::filesystem::copy_options::overwrite_existing);
        SIMPLE_LOG("SaveManager: Restored from backup: " + backupPath);
        return true;
    } catch (const std::exception& e) {
        SIMPLE_LOG("SaveManager: Backup restoration failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> SaveManager::GetSaveSlots() const {
    std::vector<std::string> slots;
    try {
        if (std::filesystem::exists(m_SaveDirectory)) {
            for (const auto& entry : std::filesystem::directory_iterator(m_SaveDirectory)) {
                if (entry.is_regular_file() && entry.path().extension() == ".sav") {
                    slots.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        SIMPLE_LOG("SaveManager: Failed to list save slots: " + std::string(e.what()));
    }
    return slots;
}

bool SaveManager::DeleteSave(const std::string& FilePath) {
    try {
        if (FileExists(FilePath)) {
            std::filesystem::remove(FilePath);
            SIMPLE_LOG("SaveManager: Deleted save: " + FilePath);
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        SIMPLE_LOG("SaveManager: Delete failed: " + std::string(e.what()));
        return false;
    }
}

uint32_t SaveManager::CalculateCRC32(const uint8_t* Data, size_t Size) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < Size; ++i) {
        crc = (crc >> 8) ^ CRC32_TABLE[(crc ^ Data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

bool SaveManager::FileExists(const std::string& FilePath) const {
    return std::filesystem::exists(FilePath);
}

bool SaveManager::CreateDirectory(const std::string& Path) const {
    try {
        std::filesystem::create_directories(Path);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace Solstice::Core
