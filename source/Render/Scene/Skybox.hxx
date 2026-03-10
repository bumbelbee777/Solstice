#pragma once

#include <Solstice.hxx>
#include <bgfx/bgfx.h>
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <memory>
#include <vector>
#include "Camera.hxx"
#include <Render/Assets/Mesh.hxx>
#include <Core/Debug.hxx>

namespace Solstice::Render {

// Sky preset types for quick configuration
enum class SkyPreset {
    Dawn,      // Early morning (6 AM)
    Noon,      // Midday (12 PM)
    Dusk,      // Evening (6 PM)
    Night,     // Midnight (12 AM)
    Overcast,  // Cloudy/overcast
    Clear      // Clear blue sky
};

// Skybox rendering class
// Generates and renders a cubemap skybox
class SOLSTICE_API Skybox {
public:
    Skybox();
    ~Skybox();

    // Initialize skybox with procedural cubemap generation
    // Resolution: size of each cubemap face (power of 2, e.g., 512)
    void Initialize(uint32_t resolution = 512);

    // Shutdown and cleanup resources
    void Shutdown();

    // Render the skybox (should be called before scene rendering)
    // Note: Inline implementation to work around Windows DLL export issues with nested namespaces
    // viewId: Optional view ID to render to (defaults to 4 for backward compatibility)
    // If rendering to scene view, should be called after BeginScenePass sets up the framebuffer
    inline void Render(const Camera& camera, bgfx::ProgramHandle program, unsigned int width, unsigned int height, bgfx::ViewId viewId = 4) {
        if (!m_Initialized || !bgfx::isValid(program) || !m_SkyboxMesh) {
            return;
        }

        // Set view rect
        bgfx::setViewRect(viewId, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));
        bgfx::setViewClear(viewId, BGFX_CLEAR_NONE); // Don't clear, render before scene

        // Get view matrix without translation (skybox follows camera but doesn't move)
        Math::Matrix4 view = camera.GetViewMatrix();
        // Remove translation
        view.M[3][0] = 0.0f;
        view.M[3][1] = 0.0f;
        view.M[3][2] = 0.0f;
        view.M[3][3] = 1.0f;

        // Get projection matrix
        Math::Matrix4 proj = Math::Matrix4::Perspective(
            camera.GetZoom() * 0.0174533f, // Convert to radians
            static_cast<float>(width) / static_cast<float>(height),
            0.1f,
            1000.0f
        );

        // Convert to column-major for BGFX
        Math::Matrix4 viewT = view.Transposed();
        Math::Matrix4 projT = proj.Transposed();

        // Set view transform
        bgfx::setViewTransform(viewId, &viewT.M[0][0], &projT.M[0][0]);

        // Set cubemap texture
        static bgfx::UniformHandle s_texSkybox = bgfx::createUniform("s_texSkybox", bgfx::UniformType::Sampler);
        bgfx::setTexture(0, s_texSkybox, m_CubemapTexture);

        // Set identity transform (skybox is centered at origin)
        float identity[16] = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
        bgfx::setTransform(identity);

        // Set up vertex/index buffers
        // Create buffers if they don't exist
        if (m_SkyboxMesh->VertexBufferHandle.Handle == 0xFFFF) {
            // Create vertex buffer
            bgfx::VertexLayout layout;
            layout
                .begin(bgfx::getRendererType())
                .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
                .end();

            const bgfx::Memory* mem = bgfx::copy(m_SkyboxMesh->Vertices.data(),
                static_cast<uint32_t>(m_SkyboxMesh->Vertices.size() * sizeof(QuantizedVertex)));
            bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(mem, layout);
            m_SkyboxMesh->VertexBufferHandle.Handle = vbh.idx;
        }

        if (m_SkyboxMesh->IndexBufferHandle.Handle == 0xFFFF) {
            // Create index buffer
            const bgfx::Memory* mem = bgfx::copy(m_SkyboxMesh->Indices.data(),
                static_cast<uint32_t>(m_SkyboxMesh->Indices.size() * sizeof(uint32_t)));
            bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(mem, BGFX_BUFFER_INDEX32);
            m_SkyboxMesh->IndexBufferHandle.Handle = ibh.idx;
        }

        bgfx::VertexBufferHandle vbh = { m_SkyboxMesh->VertexBufferHandle.Handle };
        bgfx::IndexBufferHandle ibh = { m_SkyboxMesh->IndexBufferHandle.Handle };

        if (!bgfx::isValid(vbh) || !bgfx::isValid(ibh)) {
            return;
        }

        bgfx::setVertexBuffer(0, vbh);
        bgfx::setIndexBuffer(ibh);

        // Skybox render state: no depth write, depth test less-equal
        // NOTE: Using CULL_CW because the cube is inverted (normals flipped inward)
        // This ensures the skybox faces are visible from inside the cube
        uint64_t state = BGFX_STATE_WRITE_RGB
            | BGFX_STATE_WRITE_A
            | BGFX_STATE_DEPTH_TEST_LEQUAL
            | BGFX_STATE_CULL_CW;  // Changed from CCW to CW for inverted cube

        bgfx::setState(state);
        bgfx::submit(viewId, program);
    }

    // Get the cubemap texture handle
    bgfx::TextureHandle GetCubemap() const { return m_CubemapTexture; }

    // Get the skybox mesh
    Mesh* GetMesh() const { return m_SkyboxMesh.get(); }

    // Check if skybox is initialized
    bool IsInitialized() const { return m_Initialized; }

    // Time-of-day control
    // hour: Time in 24-hour format (0-24)
    void SetTimeOfDay(float hour);

    // Sun parameters
    void SetSunDirection(const Math::Vec3& direction);
    void SetSunColor(const Math::Vec3& color);
    void SetSunIntensity(float intensity);
    void SetSunSize(float size);

    // Atmospheric parameters
    void SetHorizonColor(const Math::Vec3& color);
    void SetZenithColor(const Math::Vec3& color);
    void SetCloudDensity(float density);
    void SetAtmosphereThickness(float thickness);

    // Preset configurations
    void SetPreset(SkyPreset preset);

    // Regenerate cubemap with current parameters
    void Regenerate();

    // Getters
    float GetTimeOfDay() const { return m_TimeOfDay; }
    Math::Vec3 GetSunDirection() const { return m_SunDirection; }
    Math::Vec3 GetSunColor() const { return m_SunColor; }
    float GetSunIntensity() const { return m_SunIntensity; }
    float GetSunSize() const { return m_SunSize; }
    Math::Vec3 GetHorizonColor() const { return m_HorizonColor; }
    Math::Vec3 GetZenithColor() const { return m_ZenithColor; }
    float GetCloudDensity() const { return m_CloudDensity; }
    float GetAtmosphereThickness() const { return m_AtmosphereThickness; }

private:
    // Generate a single cubemap face
    // Face: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    void GenerateCubemapFace(uint32_t face, uint32_t resolution, std::vector<uint8_t>& pixels);

    // Generate procedural sky color for a direction
    Math::Vec3 GenerateSkyColor(const Math::Vec3& direction);

    // Create skybox mesh (inverted cube)
    void CreateSkyboxMesh();

    bool m_Initialized;
    bgfx::TextureHandle m_CubemapTexture;
    std::unique_ptr<Mesh> m_SkyboxMesh;
    uint32_t m_Resolution;

    // Sky parameters
    float m_TimeOfDay; // 0-24 hours
    Math::Vec3 m_SunDirection;
    Math::Vec3 m_SunColor;
    float m_SunIntensity;
    float m_SunSize;

    // Atmospheric parameters
    Math::Vec3 m_HorizonColor;
    Math::Vec3 m_ZenithColor;
    float m_CloudDensity;
    float m_AtmosphereThickness;
};

} // namespace Solstice::Render
