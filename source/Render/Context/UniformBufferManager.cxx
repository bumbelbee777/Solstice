#include "UniformBufferManager.hxx"
#include <Core/Debug.hxx>
#include <cstring>

namespace Solstice::Render {

UniformBufferManager::UniformBufferManager()
    : m_Batching(false)
    , m_BufferSize(256)
    , m_Initialized(false)
{
}

UniformBufferManager::~UniformBufferManager() {
    Shutdown();
}

void UniformBufferManager::Initialize(uint32_t bufferSize) {
    if (m_Initialized) return;
    m_BufferSize = bufferSize;
    m_Initialized = true;
    SIMPLE_LOG("UniformBufferManager: Initialized with buffer size " + std::to_string(bufferSize) + " vec4s");
}

void UniformBufferManager::Shutdown() {
    if (!m_Initialized) return;
    
    for (auto& [name, buffer] : m_Buffers) {
        if (bgfx::isValid(buffer.Handle)) {
            bgfx::destroy(buffer.Handle);
        }
    }
    m_Buffers.clear();
    m_Initialized = false;
}

bgfx::UniformHandle UniformBufferManager::CreateUniformBuffer(const std::string& name, uint32_t numVec4s) {
    if (m_Buffers.find(name) != m_Buffers.end()) {
        return m_Buffers[name].Handle;
    }

    // Create uniform buffer (BGFX uses uniform handles, not actual UBOs in all backends)
    // For packing, we'll use vec4 arrays
    bgfx::UniformHandle handle = bgfx::createUniform(
        name.c_str(),
        bgfx::UniformType::Vec4,
        numVec4s
    );

    if (bgfx::isValid(handle)) {
        UniformBuffer buffer;
        buffer.Handle = handle;
        buffer.Data.resize(numVec4s * sizeof(Math::Vec4));
        buffer.Dirty = false;
        m_Buffers[name] = std::move(buffer);
    }

    return handle;
}

bgfx::UniformHandle UniformBufferManager::GetUniformBuffer(const std::string& name) {
    auto it = m_Buffers.find(name);
    if (it != m_Buffers.end()) {
        return it->second.Handle;
    }
    return BGFX_INVALID_HANDLE;
}

void UniformBufferManager::UpdateUniformBuffer(const std::string& name, const void* data, uint32_t size) {
    auto it = m_Buffers.find(name);
    if (it == m_Buffers.end()) {
        SIMPLE_LOG("UniformBufferManager: Buffer '" + name + "' not found");
        return;
    }

    if (size > it->second.Data.size()) {
        SIMPLE_LOG("UniformBufferManager: Update size exceeds buffer size for '" + name + "'");
        return;
    }

    std::memcpy(it->second.Data.data(), data, size);
    it->second.Dirty = true;

    // If not batching, apply immediately
    if (!m_Batching) {
        bgfx::setUniform(it->second.Handle, it->second.Data.data(), 
                        static_cast<uint16_t>(size / sizeof(Math::Vec4)));
        it->second.Dirty = false;
    }
}

void UniformBufferManager::BeginBatch() {
    m_Batching = true;
    m_BatchUpdates.clear();
}

void UniformBufferManager::AddUniformUpdate(const std::string& name, const void* data, uint32_t size) {
    if (!m_Batching) {
        UpdateUniformBuffer(name, data, size);
        return;
    }

    auto it = m_Buffers.find(name);
    if (it == m_Buffers.end()) {
        SIMPLE_LOG("UniformBufferManager: Buffer '" + name + "' not found for batching");
        return;
    }

    std::vector<uint8_t> updateData(size);
    std::memcpy(updateData.data(), data, size);
    m_BatchUpdates.push_back({name, std::move(updateData)});
}

void UniformBufferManager::EndBatch() {
    if (!m_Batching) return;

    // Apply all batched updates
    for (const auto& [name, data] : m_BatchUpdates) {
        auto it = m_Buffers.find(name);
        if (it != m_Buffers.end()) {
            std::memcpy(it->second.Data.data(), data.data(), data.size());
            bgfx::setUniform(it->second.Handle, it->second.Data.data(),
                            static_cast<uint16_t>(data.size() / sizeof(Math::Vec4)));
            it->second.Dirty = false;
        }
    }

    m_BatchUpdates.clear();
    m_Batching = false;
}

void UniformBufferManager::SetUniform(bgfx::UniformHandle handle, const void* value, uint16_t num) {
    bgfx::setUniform(handle, value, num);
}

} // namespace Solstice::Render

