#include "ShaderLoader.hxx"
#include <Core/Debug.hxx>
#include <fstream>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace Solstice::Render {

bgfx::ShaderHandle ShaderLoader::LoadShader(const std::string& name) {
    // Unified search paths (combines all paths from different implementations)
    const std::vector<std::string> searchPaths = {
        "shaders/",
        "source/Shaders/bin/",
        "../source/Shaders/bin/",
        "../../source/Shaders/bin/",
        "../../../source/Shaders/bin/",
        "../../../../source/Shaders/bin/"
    };

    std::string finalPath;
    std::ifstream file;

    // Try each search path
    for (const auto& prefix : searchPaths) {
        std::string path = prefix + name;
        file.open(path, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            finalPath = path;
            SIMPLE_LOG("ShaderLoader: Found shader " + name + " at " + path);
            break;
        }
    }

    // If not found, log detailed error information
    if (!file.is_open()) {
        char cwd[1024];
#if defined(_WIN32)
        GetCurrentDirectoryA(sizeof(cwd), cwd);
#else
        getcwd(cwd, sizeof(cwd));
#endif
        SIMPLE_LOG("ShaderLoader: Failed to open shader file: " + name);
        SIMPLE_LOG("  Current Working Directory: " + std::string(cwd));
        SIMPLE_LOG("  Tried paths:");
        for (const auto& prefix : searchPaths) {
            SIMPLE_LOG("    " + prefix + name);
        }
        return BGFX_INVALID_HANDLE;
    }

    // Read file size and allocate memory
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Allocate bgfx memory (size + 1 for null terminator)
    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size) + 1);
    if (!mem || !mem->data) {
        SIMPLE_LOG("ShaderLoader: ERROR - bgfx::alloc failed for " + name);
        file.close();
        return BGFX_INVALID_HANDLE;
    }

    // Read directly into bgfx memory
    file.read(reinterpret_cast<char*>(mem->data), size);
    mem->data[mem->size - 1] = '\0'; // Null terminate
    file.close();

    // Create shader
    bgfx::ShaderHandle shader = bgfx::createShader(mem);

    if (!bgfx::isValid(shader)) {
        SIMPLE_LOG("ShaderLoader: ERROR - bgfx::createShader failed for " + name);
    }

    return shader;
}

} // namespace Solstice::Render
