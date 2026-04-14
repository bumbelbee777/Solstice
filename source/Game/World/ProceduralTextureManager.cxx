#include "World/ProceduralTextureManager.hxx"
#include "../../Core/Debug/Debug.hxx"
#include "../../Arzachel/MaterialPresets.hxx"
#include <fstream>
#include <sstream>
#include <thread>
#include <set>

namespace Solstice::Game {

ProceduralTextureManager::ProceduralTextureManager() {
}

ProceduralTextureManager::~ProceduralTextureManager() {
    Shutdown();
}

void ProceduralTextureManager::Initialize(const std::vector<TextureConfig>& TextureConfigs, const std::string& CacheDirectory) {
    m_TextureConfigs = TextureConfigs;
    m_CacheDirectory = CacheDirectory;

    SIMPLE_LOG("ProceduralTextureManager: Starting async texture generation...");

    // Ensure cache directory exists
    std::filesystem::create_directories(m_CacheDirectory);

    // Set initial state
    m_TextureGenState = TextureGenState::InProgress;
    m_TextureGenProgress = 0.0f;
    m_TexturesCompleted = 0;
    m_TexturesCreated = 0;
    {
        Core::LockGuard ArrayLock(m_TextureArrayLock);
        m_Textures.clear();
        m_Textures.resize(TextureConfigs.size(), BGFX_INVALID_HANDLE);
    }
    m_TexturesReady.store(false, std::memory_order_relaxed);

    // Safety check: ensure JobSystem is initialized
    try {
        Core::JobSystem::Instance().Initialize();
        SIMPLE_LOG("JobSystem ready for texture generation");
    } catch (const std::exception& e) {
        SIMPLE_LOG("ERROR: Failed to initialize JobSystem: " + std::string(e.what()));
        m_TextureGenState = TextureGenState::Error;
        return;
    } catch (...) {
        SIMPLE_LOG("JobSystem initialization check completed");
    }

    const size_t TotalTextures = TextureConfigs.size();
    m_TextureGenFutures.clear();
    m_TextureGenFutures.reserve(TotalTextures);

    // Submit async tasks for each texture
    for (size_t I = 0; I < TotalTextures; ++I) {
        try {
            std::future<void> Future = Core::JobSystem::Instance().SubmitAsync(
                [this, I, TotalTextures]() {
                    GenerateTextureAsync(I, m_TextureConfigs[I], TotalTextures);
                });
            {
                Core::LockGuard Lock(m_TextureGenFuturesLock);
                m_TextureGenFutures.push_back(std::move(Future));
            }
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Failed to submit async task for texture " + m_TextureConfigs[I].Name + ": " + std::string(e.what()));
            m_TextureGenState = TextureGenState::Error;
            continue;
        } catch (...) {
            SIMPLE_LOG("ERROR: Unknown exception submitting async task for texture " + m_TextureConfigs[I].Name);
            m_TextureGenState = TextureGenState::Error;
            continue;
        }
    }

    // Wait a tiny bit to ensure all async tasks are submitted
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Start completion checker
    StartCompletionChecker(TotalTextures);
}

void ProceduralTextureManager::GenerateTextureAsync(size_t Index, const TextureConfig& Config, size_t TotalTextures) {
    try {

        // Update current texture name
        {
            Core::LockGuard Lock(m_TextureNameLock);
            m_CurrentTextureName = Config.Name;
        }

        // Generate pixel data
        Arzachel::TextureData Data;
        std::string CachePath = m_CacheDirectory + "/" + Config.Name + ".json";
        bool LoadedFromCache = false;

        try {
            if (std::filesystem::exists(CachePath)) {
                std::ifstream File(CachePath);
                if (File.is_open()) {
                    std::stringstream Buffer;
                    Buffer << File.rdbuf();
                    try {
                        Core::JSONValue JSON = Core::JSONParser::Parse(Buffer.str());
                        Data = Arzachel::MaterialSerializer::DeserializeTexture(JSON);
                        LoadedFromCache = true;
                    } catch (const std::exception& e) {
                        SIMPLE_LOG("Failed to parse cache for " + Config.Name + ": " + e.what());
                    } catch (...) {
                        SIMPLE_LOG("Failed to parse cache for: " + Config.Name);
                    }
                }
            }
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception checking cache for " + Config.Name + ": " + std::string(e.what()));
        }

        if (!LoadedFromCache) {
            try {
                if (Config.CustomGenerator) {
                    Data = Config.CustomGenerator();
                } else {
                    // Use default noise generation
                    Data = Arzachel::ProceduralTexture::GenerateNoise(
                        Config.Resolution, Config.Octaves, Config.Scale, Arzachel::Seed(Config.Seed));
                }

                // Save to cache
                try {
                    std::ofstream File(CachePath);
                    if (File.is_open()) {
                        Core::JSONValue JSON = Arzachel::MaterialSerializer::SerializeTexture(Data);
                        File << JSON.Stringify(true);
                    }
                } catch (const std::exception& e) {
                    SIMPLE_LOG("WARNING: Failed to save cache for " + Config.Name + ": " + std::string(e.what()));
                }
            } catch (const std::exception& e) {
                SIMPLE_LOG("ERROR: Exception generating texture " + Config.Name + ": " + std::string(e.what()));
                throw;
            }
        }

        if (Data.Pixels.empty()) {
            SIMPLE_LOG("ERROR: Generated empty pixel data for " + Config.Name);
            m_TextureGenState = TextureGenState::Error;
            return;
        }

        if (Data.Width == 0 || Data.Height == 0) {
            SIMPLE_LOG("ERROR: Invalid texture dimensions for " + Config.Name);
            m_TextureGenState = TextureGenState::Error;
            return;
        }

        // Store pixel data for main thread
        try {
            Core::LockGuard Lock(m_TextureDataLock);
            TexturePixelData PendingData;
            PendingData.Pixels = std::move(Data.Pixels);
            PendingData.Resolution = Data.Width;
            PendingData.Name = Config.Name;
            PendingData.Index = Index;
            m_PendingTextureData.push_back(std::move(PendingData));
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception storing pixel data for " + Config.Name + ": " + std::string(e.what()));
            m_TextureGenState = TextureGenState::Error;
            return;
        }

        // Update progress
        try {
            int CompletedCount = m_TexturesCompleted.fetch_add(1, std::memory_order_relaxed) + 1;
            m_TextureGenProgress = static_cast<float>(CompletedCount) / static_cast<float>(TotalTextures) * 0.5f;
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception updating progress for " + Config.Name + ": " + std::string(e.what()));
        }
    } catch (const std::exception& E) {
        SIMPLE_LOG("ERROR: Failed to generate texture " + Config.Name + ": " + E.what());
        m_TextureGenState = TextureGenState::Error;
    } catch (...) {
        SIMPLE_LOG("ERROR: Unknown exception while generating texture " + Config.Name);
        m_TextureGenState = TextureGenState::Error;
    }
}

void ProceduralTextureManager::StartCompletionChecker(size_t TotalTextures) {
    try {
        Core::JobSystem::Instance().SubmitAsync([this, TotalTextures]() {
            try {
                // Wait for all pixel data generation to complete
                int WaitForFutures = 0;
                while (WaitForFutures < 50) { // Max 5 seconds
                    {
                        Core::LockGuard Lock(m_TextureGenFuturesLock);
                        if (m_TextureGenFutures.size() >= TotalTextures) {
                            break;
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    WaitForFutures++;
                }

                // Wait for all futures
                {
                    Core::LockGuard Lock(m_TextureGenFuturesLock);
                    const size_t FuturesCount = m_TextureGenFutures.size();
                    if (FuturesCount != TotalTextures) {
                        SIMPLE_LOG("WARNING: Expected " + std::to_string(TotalTextures) + " futures, got " + std::to_string(FuturesCount));
                    }
                    for (size_t I = 0; I < FuturesCount && I < TotalTextures; ++I) {
                        try {
                            m_TextureGenFutures[I].wait();
                        } catch (const std::exception& e) {
                            SIMPLE_LOG("ERROR: Exception waiting for texture generation future " + std::to_string(I) + ": " + std::string(e.what()));
                        } catch (...) {
                            // Error already logged
                        }
                    }
                }

                // Wait for main thread to process all pending texture data
                int MaxWaitIterations = 100; // Max 10 seconds
                int WaitIterations = 0;
                while (WaitIterations < MaxWaitIterations) {
                    int CreatedCount = m_TexturesCreated.load(std::memory_order_acquire);
                    if (CreatedCount >= static_cast<int>(TotalTextures)) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    WaitIterations++;
                }

                // Check if all textures were created
                int CreatedCountFinal = m_TexturesCreated.load(std::memory_order_acquire);
                TextureGenState CurrentState = m_TextureGenState.load(std::memory_order_acquire);
                if (CreatedCountFinal >= static_cast<int>(TotalTextures) && CurrentState == TextureGenState::InProgress) {
                    m_TextureGenState = TextureGenState::Completed;
                    m_TextureGenProgress = 1.0f;
                    m_TexturesReady.store(true, std::memory_order_release);
                    SIMPLE_LOG("ProceduralTextureManager: All textures generated (" +
                              std::to_string(CreatedCountFinal) + "/" + std::to_string(TotalTextures) + ")");
                } else {
                    if (CurrentState != TextureGenState::Error) {
                        m_TextureGenState = TextureGenState::Error;
                    }
                    SIMPLE_LOG("ERROR: Not all textures were created (" +
                              std::to_string(CreatedCountFinal) + "/" + std::to_string(TotalTextures) + ")");
                }
            } catch (const std::exception& e) {
                SIMPLE_LOG("ERROR: Exception in completion checker: " + std::string(e.what()));
                m_TextureGenState = TextureGenState::Error;
            } catch (...) {
                SIMPLE_LOG("ERROR: Unknown exception in completion checker");
                m_TextureGenState = TextureGenState::Error;
            }
        });
    } catch (const std::exception& e) {
        SIMPLE_LOG("ERROR: Failed to submit completion checker task: " + std::string(e.what()));
        m_TextureGenState = TextureGenState::Error;
    } catch (...) {
        SIMPLE_LOG("ERROR: Unknown exception submitting completion checker task");
        m_TextureGenState = TextureGenState::Error;
    }
}

bool ProceduralTextureManager::ProcessPendingTextures() {
    TextureGenState CurrentState = m_TextureGenState.load(std::memory_order_acquire);

    // Only process textures after first frame is rendered (BGFX requirement)
    // Note: m_FirstFrameRendered should be set by the caller (BlizzardGame::Render)
    if (CurrentState != TextureGenState::InProgress) {
        return false;
    }

    TexturePixelData Data;
    bool HasData = false;
    {
        Core::LockGuard Lock(m_TextureDataLock);
        if (!m_PendingTextureData.empty()) {
            Data = std::move(m_PendingTextureData[0]);
            m_PendingTextureData.erase(m_PendingTextureData.begin());
            HasData = true;
        }
    }

    if (!HasData) return false;

    try {

        // Validate index
        {
            Core::LockGuard ArrayLock(m_TextureArrayLock);
            if (Data.Index >= m_Textures.size()) {
                SIMPLE_LOG("ERROR: Texture index " + std::to_string(Data.Index) + " out of range, resizing...");
                m_Textures.resize(Data.Index + 1, BGFX_INVALID_HANDLE);
            }
        }

        if (Data.Pixels.empty()) {
            SIMPLE_LOG("ERROR: Empty pixel data for texture " + Data.Name);
            return false;
        }

        if (Data.Resolution == 0) {
            SIMPLE_LOG("ERROR: Invalid resolution (0) for texture " + Data.Name);
            return false;
        }

        uint32_t ExpectedSize = Data.Resolution * Data.Resolution * 4; // RGBA8
        if (Data.Pixels.size() != ExpectedSize) {
            SIMPLE_LOG("ERROR: Pixel data size mismatch for " + Data.Name);
            return false;
        }

        // Create bgfx texture
        Arzachel::TextureData TexData;
        TexData.Width = Data.Resolution;
        TexData.Height = Data.Resolution;
        TexData.Pixels = Data.Pixels; // Copy for bgfx::copy

        bgfx::TextureHandle Texture;
        try {
            Texture = Arzachel::ProceduralTexture::UploadToGPU(TexData);
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception in UploadToGPU for " + Data.Name + ": " + e.what());
            return false;
        } catch (...) {
            SIMPLE_LOG("ERROR: Unknown exception in UploadToGPU for " + Data.Name);
            return false;
        }

        if (!bgfx::isValid(Texture)) {
            SIMPLE_LOG("ERROR: Failed to create bgfx texture for " + Data.Name);
            return false;
        }

        // Store texture
        {
            Core::LockGuard ArrayLock(m_TextureArrayLock);
            if (Data.Index >= m_Textures.size()) {
                m_Textures.resize(Data.Index + 1, BGFX_INVALID_HANDLE);
            }
            m_Textures[Data.Index] = Texture;
        }


        // Increment counter
        int CreatedCount = m_TexturesCreated.fetch_add(1, std::memory_order_relaxed) + 1;

        // Update progress
        const size_t TotalTextures = m_TextureConfigs.size();
        float PixelGenProgress = static_cast<float>(m_TexturesCompleted.load(std::memory_order_acquire)) / static_cast<float>(TotalTextures) * 0.5f;
        float UploadProgress = static_cast<float>(CreatedCount) / static_cast<float>(TotalTextures) * 0.5f;
        m_TextureGenProgress = PixelGenProgress + UploadProgress;

        if (m_TextureGenProgress > 1.0f) {
            m_TextureGenProgress = 1.0f;
        }

        return true;
    } catch (const std::exception& e) {
        SIMPLE_LOG("ERROR: Exception creating texture " + Data.Name + ": " + std::string(e.what()));
        return false;
    } catch (...) {
        SIMPLE_LOG("ERROR: Unknown exception creating texture " + Data.Name);
        return false;
    }
}

void ProceduralTextureManager::Finalize(Render::SoftwareRenderer* Renderer, Core::MaterialLibrary* MaterialLibrary,
                                         const std::vector<MaterialAssignment>& MaterialAssignments) {
    using namespace Solstice::Arzachel;

    if (!m_TexturesReady.load(std::memory_order_acquire)) {
        SIMPLE_LOG("ProceduralTextureManager::Finalize: Textures not ready yet");
        return;
    }

    size_t TextureCount = 0;
    {
        Core::LockGuard Lock(m_TextureArrayLock);
        TextureCount = m_Textures.size();
    }

    if (TextureCount < MaterialAssignments.size()) {
        SIMPLE_LOG("ERROR: Not enough textures (count=" + std::to_string(TextureCount) + ", need " + std::to_string(MaterialAssignments.size()) + ")");
        return;
    }

    // Validate all textures
    {
        Core::LockGuard Lock(m_TextureArrayLock);
        for (size_t I = 0; I < m_Textures.size(); ++I) {
            if (!bgfx::isValid(m_Textures[I])) {
                SIMPLE_LOG("ERROR: Texture " + std::to_string(I) + " is invalid, cannot finalize");
                return;
            }
        }
    }

    if (!Renderer || !MaterialLibrary) {
        SIMPLE_LOG("ERROR: Renderer or MaterialLibrary is null");
        return;
    }

    // Pre-create directories for all materials to avoid repeated filesystem calls
    std::set<std::filesystem::path> DirectoriesToCreate;
    for (const auto& Assignment : MaterialAssignments) {
        if (!Assignment.MaterialPath.empty()) {
            auto DirPath = std::filesystem::path(Assignment.MaterialPath).parent_path();
            if (!DirPath.empty()) {
                DirectoriesToCreate.insert(DirPath);
            }
        }
    }

    // Create all directories at once (much faster than creating them one by one)
    for (const auto& DirPath : DirectoriesToCreate) {
        try {
            std::filesystem::create_directories(DirPath);
        } catch (const std::exception& e) {
            SIMPLE_LOG("WARNING: Could not create directory " + DirPath.string() + ": " + e.what());
        }
    }

    // Register textures and assign to materials
    for (size_t I = 0; I < MaterialAssignments.size(); ++I) {
        const auto& Assignment = MaterialAssignments[I];
        if (I >= m_Textures.size()) continue;

        bgfx::TextureHandle Texture;
        {
            Core::LockGuard Lock(m_TextureArrayLock);
            Texture = m_Textures[I];
        }

        if (!bgfx::isValid(Texture)) continue;

        // Register texture
        uint16_t TexIndex = Renderer->GetTextureRegistry().Register(Texture);
        SIMPLE_LOG("Registered texture " + std::to_string(I) + " at index " + std::to_string(TexIndex));

        // Store registered index for GetTextureHandle()
        m_RegisteredIndices[I] = TexIndex;

        // Update material
        Core::Material* Mat = MaterialLibrary->GetMaterial(Assignment.MaterialID);
        if (Mat) {
            // Try to load from disk if path specified (only check once, directory already exists)
            bool LoadedFromDisk = false;
            if (!Assignment.MaterialPath.empty()) {
                try {
                    if (std::filesystem::exists(Assignment.MaterialPath)) {
                    Core::Material LoadedMat;
                    if (MaterialSerializer::LoadFromFile(Assignment.MaterialPath, LoadedMat)) {
                        *Mat = LoadedMat;
                        LoadedFromDisk = true;
                        SIMPLE_LOG("Loaded material from disk: " + Assignment.MaterialPath);
                        }
                    }
                } catch (const std::exception& e) {
                    SIMPLE_LOG("ERROR: Exception loading material: " + std::string(e.what()));
                }
            }

            // Update with textures if not loaded from disk
            if (!LoadedFromDisk) {
                Mat->SetAlbedoColor(Assignment.AlbedoColor, Assignment.Roughness);
                Mat->AlbedoTexIndex = TexIndex;
                if (Assignment.AlbedoTexIndex2 != 0xFFFF) Mat->AlbedoTexIndex2 = Assignment.AlbedoTexIndex2;
                if (Assignment.AlbedoTexIndex3 != 0xFFFF) Mat->AlbedoTexIndex3 = Assignment.AlbedoTexIndex3;
                Mat->TextureBlendMode = Assignment.TextureBlendMode;
                Mat->SetTextureBlendFactor(Assignment.TextureBlendFactor);
            } else {
                // Update texture indices even if loaded from disk
                Mat->AlbedoTexIndex = TexIndex;
                if (Assignment.AlbedoTexIndex2 != 0xFFFF) Mat->AlbedoTexIndex2 = Assignment.AlbedoTexIndex2;
                if (Assignment.AlbedoTexIndex3 != 0xFFFF) Mat->AlbedoTexIndex3 = Assignment.AlbedoTexIndex3;
            }

            // Save material if path specified (defer to avoid blocking - save in background or skip if already exists)
            if (!Assignment.MaterialPath.empty() && !LoadedFromDisk) {
                try {
                    // Only save if file doesn't exist (avoid overwriting)
                    if (!std::filesystem::exists(Assignment.MaterialPath)) {
                    MaterialSerializer::SaveToFile(Assignment.MaterialPath, *Mat);
                    }
                } catch (const std::exception& e) {
                    SIMPLE_LOG("ERROR: Exception saving material: " + std::string(e.what()));
                }
            }
        }
    }

    SIMPLE_LOG("ProceduralTextureManager::Finalize: Textures registered and materials updated");
}

std::string ProceduralTextureManager::GetCurrentTextureName() const {
    Core::LockGuard Lock(m_TextureNameLock);
    return m_CurrentTextureName;
}

bgfx::TextureHandle ProceduralTextureManager::GetTexture(size_t Index) const {
    Core::LockGuard Lock(m_TextureArrayLock);
    if (Index < m_Textures.size()) {
        return m_Textures[Index];
    }
    return BGFX_INVALID_HANDLE;
}

Core::TextureHandle ProceduralTextureManager::GetTextureHandle(size_t Index) const {
    // Check if texture was registered
    auto It = m_RegisteredIndices.find(Index);
    if (It != m_RegisteredIndices.end()) {
        return Core::TextureHandle(It->second);
    }

    // If not registered yet, return invalid handle
    return Core::TextureHandle::Invalid();
}

void ProceduralTextureManager::Shutdown() {
    // Wait for all async tasks to complete
    if (m_TextureGenState.load(std::memory_order_acquire) == TextureGenState::InProgress) {
        SIMPLE_LOG("ProceduralTextureManager: Waiting for texture generation to complete...");
        {
            Core::LockGuard Lock(m_TextureGenFuturesLock);
            for (auto& Future : m_TextureGenFutures) {
                if (Future.valid()) {
                    try {
                        Future.wait();
                    } catch (...) {
                        // Ignore exceptions during shutdown
                    }
                }
            }
        }
        int MaxWait = 100;
        while (m_TextureGenState.load(std::memory_order_acquire) == TextureGenState::InProgress && MaxWait > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            MaxWait--;
        }
    }
}

} // namespace Solstice::Game
