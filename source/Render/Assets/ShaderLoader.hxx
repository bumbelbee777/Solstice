#pragma once

#include <Solstice.hxx>
#include <bgfx/bgfx.h>
#include <string>

namespace Solstice::Render {

// Centralized shader loading utility
// Replaces duplicated shader loading code across the codebase
class SOLSTICE_API ShaderLoader {
public:
    // Load a shader binary from disk
    // Searches multiple paths and returns bgfx::ShaderHandle or BGFX_INVALID_HANDLE
    static bgfx::ShaderHandle LoadShader(const std::string& name);
};

} // namespace Solstice::Render
