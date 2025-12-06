#pragma once
#include <vector>
#include <cstdint>
#include <bgfx/bgfx.h>

namespace Solstice::Render {
class Framebuffer {
public:
    int Width, Height;
    bgfx::FrameBufferHandle Handle{bgfx::kInvalidHandle};

    Framebuffer(int Width, int Height);
    ~Framebuffer();

    void Resize(int NewWidth, int NewHeight);
    
    // Methods removed/stubbed for BGFX migration
    // void Clear(const glm::vec4& Color);
    // void ClearColorBuffer();
    // void ClearDepthBuffer();
    // void SetPixel(int x, int y, const glm::vec4& Color);
    // bool IsInside(int X, int Y) const;
    // const uint32_t* GetColorData() const;
    // const float* GetDepthData() const;
};
} // namespace Solstice::Render