#pragma once

#include <Render/Camera.hxx>
#include <Render/Framebuffer.hxx>
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <cstdint>
#include <vector>
#include <memory>

namespace Solstice::Render {

struct RenderTarget {
    virtual ~RenderTarget() = default;
    
    virtual void Bind() const = 0;
    virtual void Unbind() const = 0;
    virtual Math::Vec2 GetSize() const = 0;
    virtual void Resize(Math::Vec2 NewSize) = 0;
    
    // Optional: Access to underlying BGFX handles
    virtual uint16_t GetFramebufferHandle() const { return UINT16_MAX; }
    virtual uint16_t GetColorTextureHandle() const { return UINT16_MAX; }
    virtual uint16_t GetDepthTextureHandle() const { return UINT16_MAX; }
};

struct Viewport {
    Math::Vec2 Position{0.0f, 0.0f};
    Math::Vec2 Size{0.0f, 0.0f};
    std::shared_ptr<::Solstice::Render::Camera> Camera{nullptr};
    std::shared_ptr<RenderTarget> Target{nullptr};
    Math::Vec4 ClearColor{0.1f, 0.1f, 0.1f, 1.0f};
    bool IsActive{true};
    bool ClearOnBind{true};
    
    Viewport() = default;
    Viewport(const Math::Vec2& Pos, const Math::Vec2& Dims, 
             std::shared_ptr<::Solstice::Render::Camera> Cam = {},
             std::shared_ptr<RenderTarget> RT = {})
        : Position(Pos), Size(Dims), Camera(Cam), Target(RT) {}
};

struct IRenderer {

    IRenderer(int Width, int Height)  : Framebuffer(Width, Height) {}

    virtual ~IRenderer() = default;

    virtual void Clear(const Math::Vec4& Color) = 0;

    virtual void DrawTriangle(const Math::Vec3& v1, const Math::Vec3& v2, const Math::Vec3& v3
        , const Math::Vec4& Color) = 0;
    virtual void DrawLine(const Math::Vec3& Start, const Math::Vec3& End, const Math::Vec4& Color
        , float Width = 1.0f) = 0;
    virtual void DrawPoint(const Math::Vec3& Position, const Math::Vec4& Color
        , float Size = 1.0f) = 0;

    virtual void EnableDepthTest(bool Enable) = 0;
    virtual void EnableBlend(bool Enable) = 0;

    // Matrix setters
    virtual void SetProjectionMatrix(const Math::Matrix4& mat) { ProjectionMatrix = mat; }
    virtual void SetViewMatrix(const Math::Matrix4& mat) { ViewMatrix = mat; }
    virtual void SetModelMatrix(const Math::Matrix4& mat) { ModelMatrix = mat; }

protected:
    Math::Matrix4 ProjectionMatrix, ViewMatrix, ModelMatrix;
    Viewport Viewport;
    Framebuffer Framebuffer;
};

}