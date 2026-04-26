#include "Skybox.hxx"
#include <Solstice.hxx>  // Ensure SOLSTICE_EXPORTS is defined
#include <Render/Assets/Mesh.hxx>
#include "Camera.hxx"
#include <Core/Debug/Debug.hxx>
#include <bgfx/bgfx.h>

#include <Arzachel/MeshFactory.hxx>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include <bx/allocator.h>
#include <bx/file.h>
#include <bimg/decode.h>

using QuantizedVertex = Solstice::Render::QuantizedVertex;

namespace {

bool LoadImageFileToRgba8(const std::string& path, std::vector<uint8_t>& rgba, uint32_t& outW, uint32_t& outH, std::string* errOut) {
    static bx::DefaultAllocator s_Alloc;
    bx::FileReader reader;
    bx::Error berr;
    if (!reader.open(bx::FilePath(path.c_str()), &berr)) {
        if (errOut) {
            *errOut = "Skybox: cannot open image: " + path;
        }
        return false;
    }
    const uint32_t size = static_cast<uint32_t>(reader.seek(0, bx::Whence::End));
    reader.seek(0, bx::Whence::Begin);
    std::vector<uint8_t> fileData(size);
    reader.read(fileData.data(), static_cast<int32_t>(size), &berr);
    reader.close();

    bx::Error parseErr;
    bimg::ImageContainer* container =
        bimg::imageParse(&s_Alloc, fileData.data(), size, bimg::TextureFormat::RGBA8, &parseErr);
    if (!container || !parseErr.isOk()) {
        if (errOut) {
            *errOut = "Skybox: cannot decode image: " + path;
        }
        return false;
    }
    outW = container->m_width;
    outH = container->m_height;
    rgba.resize(static_cast<size_t>(container->m_size));
    std::memcpy(rgba.data(), container->m_data, rgba.size());
    bimg::imageFree(container);
    return true;
}

void BlitResizeRgbaNearest(const uint8_t* src, int sw, int sh, std::vector<uint8_t>& out, int dw, int dh) {
    out.resize(static_cast<size_t>(dw) * static_cast<size_t>(dh) * 4u);
    for (int y = 0; y < dh; ++y) {
        const int sy = std::min(sh - 1, std::max(0, y * sh / std::max(dh, 1)));
        for (int x = 0; x < dw; ++x) {
            const int sx = std::min(sw - 1, std::max(0, x * sw / std::max(dw, 1)));
            const size_t si = (static_cast<size_t>(sy) * static_cast<size_t>(sw) + static_cast<size_t>(sx)) * 4u;
            const size_t di = (static_cast<size_t>(y) * static_cast<size_t>(dw) + static_cast<size_t>(x)) * 4u;
            out[di + 0] = src[si + 0];
            out[di + 1] = src[si + 1];
            out[di + 2] = src[si + 2];
            out[di + 3] = src[si + 3];
        }
    }
}

} // namespace

namespace Solstice::Render {

Skybox::Skybox()
    : m_Initialized(false)
    , m_CubemapTexture(BGFX_INVALID_HANDLE)
    , m_Resolution(512)
    , m_TimeOfDay(12.0f) // Noon by default
    , m_SunDirection(Math::Vec3(0.5f, 1.0f, -0.5f).Normalized())
    , m_SunColor(Math::Vec3(1.0f, 0.95f, 0.9f)) // Warm white
    , m_SunIntensity(1.0f)
    , m_SunSize(0.05f)
    , m_HorizonColor(0.5f, 0.6f, 0.8f) // Light blue-gray
    , m_ZenithColor(0.2f, 0.3f, 0.5f) // Darker blue
    , m_CloudDensity(0.3f)
    , m_AtmosphereThickness(1.0f)
{
}

Skybox::~Skybox() {
    Shutdown();
}

void Skybox::Initialize(uint32_t resolution) {
    if (m_Initialized) {
        Shutdown();
    }

    m_Resolution = resolution;

    // Generate cubemap faces
    std::vector<std::vector<uint8_t>> facePixels(6);
    for (uint32_t face = 0; face < 6; face++) {
        facePixels[face].resize(resolution * resolution * 4);
        GenerateCubemapFace(face, resolution, facePixels[face]);
    }

    // Create cubemap texture (empty first, then update each face)
    m_CubemapTexture = bgfx::createTextureCube(
        static_cast<uint16_t>(resolution),
        false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_NONE,
        nullptr
    );

    // Update each cubemap face
    for (uint32_t face = 0; face < 6; face++) {
        const bgfx::Memory* mem = bgfx::copy(facePixels[face].data(), facePixels[face].size());
        bgfx::updateTextureCube(
            m_CubemapTexture,  // handle
            0,                  // layer
            static_cast<uint8_t>(face),  // side
            0,                  // mip
            0,                  // x
            0,                  // y
            static_cast<uint16_t>(resolution),  // width
            static_cast<uint16_t>(resolution),  // height
            mem                 // memory
        );
    }

    // Create skybox mesh (inverted cube)
    CreateSkyboxMesh();

    m_Initialized = true;
    SIMPLE_LOG("Skybox: Initialized with " + std::to_string(resolution) + "x" + std::to_string(resolution) + " cubemap");
}

void Skybox::Shutdown() {
    if (bgfx::isValid(m_CubemapTexture)) {
        bgfx::destroy(m_CubemapTexture);
        m_CubemapTexture = BGFX_INVALID_HANDLE;
    }
    m_SkyboxMesh.reset();
    m_Initialized = false;
    m_ImageCubemapActive = false;
}

Math::Vec3 Skybox::GenerateSkyColor(const Math::Vec3& direction) {
    float y = direction.y; // Up direction

    // Interpolate between horizon and zenith colors
    float t = (y + 1.0f) * 0.5f; // Map from [-1,1] to [0,1]
    t = std::pow(t, m_AtmosphereThickness); // Adjust gradient steepness

    Math::Vec3 skyColor = m_HorizonColor + (m_ZenithColor - m_HorizonColor) * t;

    // Add cloud-like noise
    float noiseX = direction.x * 2.0f + direction.z * 0.5f;
    float noiseY = direction.y * 2.0f + direction.z * 0.3f;
    float cloudNoise = std::sin(noiseX * 3.14159f) * std::cos(noiseY * 3.14159f) * 0.5f + 0.5f;
    cloudNoise = std::pow(cloudNoise, 2.0f);
    // Blend cloud noise into sky
    skyColor = skyColor * (1.0f - m_CloudDensity * 0.3f + cloudNoise * m_CloudDensity * 0.3f);

    // Sun disk
    float sunDot = direction.Dot(m_SunDirection);
    if (sunDot > 1.0f - m_SunSize * 3.0f) {
        float sunIntensity = (sunDot - (1.0f - m_SunSize * 3.0f)) / (m_SunSize * 3.0f);
        sunIntensity = std::pow(sunIntensity, 0.5f);
        skyColor = skyColor + m_SunColor * (sunIntensity * m_SunIntensity);
    }

    // Clamp to valid range
    skyColor.x = std::max(0.0f, std::min(1.0f, skyColor.x));
    skyColor.y = std::max(0.0f, std::min(1.0f, skyColor.y));
    skyColor.z = std::max(0.0f, std::min(1.0f, skyColor.z));

    return skyColor;
}

void Skybox::GenerateCubemapFace(uint32_t face, uint32_t resolution, std::vector<uint8_t>& pixels) {
    // Cubemap face directions
    // +X: right, -X: left, +Y: up, -Y: down, +Z: forward, -Z: back
    Math::Vec3 faceDir[6][3] = {
        { Math::Vec3(1, 0, 0), Math::Vec3(0, 0, -1), Math::Vec3(0, 1, 0) }, // +X
        { Math::Vec3(-1, 0, 0), Math::Vec3(0, 0, 1), Math::Vec3(0, 1, 0) }, // -X
        { Math::Vec3(0, 1, 0), Math::Vec3(1, 0, 0), Math::Vec3(0, 0, 1) },  // +Y
        { Math::Vec3(0, -1, 0), Math::Vec3(1, 0, 0), Math::Vec3(0, 0, -1) }, // -Y
        { Math::Vec3(0, 0, 1), Math::Vec3(1, 0, 0), Math::Vec3(0, 1, 0) },   // +Z
        { Math::Vec3(0, 0, -1), Math::Vec3(-1, 0, 0), Math::Vec3(0, 1, 0) }  // -Z
    };

    const Math::Vec3& right = faceDir[face][0];
    const Math::Vec3& up = faceDir[face][2];
    const Math::Vec3& forward = faceDir[face][1];

    for (uint32_t y = 0; y < resolution; y++) {
        for (uint32_t x = 0; x < resolution; x++) {
            // Convert pixel coordinates to direction
            float u = (static_cast<float>(x) / static_cast<float>(resolution)) * 2.0f - 1.0f;
            float v = (static_cast<float>(y) / static_cast<float>(resolution)) * 2.0f - 1.0f;

            Math::Vec3 direction = (right * u + up * v + forward).Normalized();

            // Generate sky color for this direction
            Math::Vec3 color = GenerateSkyColor(direction);

            uint32_t idx = (y * resolution + x) * 4;
            pixels[idx + 0] = static_cast<uint8_t>(color.x * 255.0f);
            pixels[idx + 1] = static_cast<uint8_t>(color.y * 255.0f);
            pixels[idx + 2] = static_cast<uint8_t>(color.z * 255.0f);
            pixels[idx + 3] = 255;
        }
    }
}

void Skybox::CreateSkyboxMesh() {
    // Create a large inverted cube (1000 unit radius)
    // Inverted so normals point inward
    float size = 1000.0f;

    m_SkyboxMesh = Solstice::Arzachel::MeshFactory::CreateCube(size);

    // Invert the cube by flipping normals
    if (m_SkyboxMesh) {
        for (auto& vertex : m_SkyboxMesh->Vertices) {
            // Flip normals
            vertex.NormalX = -vertex.NormalX;
            vertex.NormalY = -vertex.NormalY;
            vertex.NormalZ = -vertex.NormalZ;
        }

        // Mark as static and create buffers
        m_SkyboxMesh->IsStatic = true;
    }
}

void Skybox::SetTimeOfDay(float hour) {
    m_TimeOfDay = std::max(0.0f, std::min(24.0f, hour));

    // Calculate sun direction based on time of day
    // Sun rises in east (positive X), sets in west (negative X)
    // At noon (12), sun is directly overhead
    float sunAngle = (m_TimeOfDay - 6.0f) / 12.0f * 3.14159f; // 0 at 6 AM, PI at 6 PM
    float sunElevation = std::sin(sunAngle); // Height of sun
    float sunAzimuth = std::cos(sunAngle); // East-west position

    // Only show sun if it's above horizon
    if (sunElevation > 0.0f) {
        m_SunDirection = Math::Vec3(sunAzimuth, sunElevation, 0.0f).Normalized();
    } else {
        // Sun is below horizon (night)
        m_SunDirection = Math::Vec3(sunAzimuth, 0.0f, 0.0f).Normalized();
    }

    // Adjust colors based on time of day
    if (m_TimeOfDay >= 6.0f && m_TimeOfDay <= 18.0f) {
        // Daytime
        float dayFactor = 1.0f;
        if (m_TimeOfDay < 8.0f) {
            // Dawn (6-8 AM)
            dayFactor = (m_TimeOfDay - 6.0f) / 2.0f;
            m_HorizonColor = Math::Vec3(1.0f, 0.6f, 0.4f) * dayFactor + Math::Vec3(0.5f, 0.6f, 0.8f) * (1.0f - dayFactor);
            m_ZenithColor = Math::Vec3(0.3f, 0.4f, 0.6f) * dayFactor + Math::Vec3(0.2f, 0.3f, 0.5f) * (1.0f - dayFactor);
            m_SunColor = Math::Vec3(1.0f, 0.7f, 0.5f);
            m_SunIntensity = 0.5f + dayFactor * 0.5f;
        } else if (m_TimeOfDay > 16.0f) {
            // Dusk (4-6 PM)
            dayFactor = 1.0f - (m_TimeOfDay - 16.0f) / 2.0f;
            m_HorizonColor = Math::Vec3(1.0f, 0.5f, 0.3f) * (1.0f - dayFactor) + Math::Vec3(0.5f, 0.6f, 0.8f) * dayFactor;
            m_ZenithColor = Math::Vec3(0.2f, 0.2f, 0.3f) * (1.0f - dayFactor) + Math::Vec3(0.2f, 0.3f, 0.5f) * dayFactor;
            m_SunColor = Math::Vec3(1.0f, 0.6f, 0.4f);
            m_SunIntensity = 0.5f + dayFactor * 0.5f;
        } else {
            // Noon (8 AM - 4 PM)
            m_HorizonColor = Math::Vec3(0.5f, 0.6f, 0.8f);
            m_ZenithColor = Math::Vec3(0.2f, 0.3f, 0.5f);
            m_SunColor = Math::Vec3(1.0f, 0.95f, 0.9f);
            m_SunIntensity = 1.0f;
        }
    } else {
        // Nighttime
        m_HorizonColor = Math::Vec3(0.1f, 0.1f, 0.15f);
        m_ZenithColor = Math::Vec3(0.05f, 0.05f, 0.1f);
        m_SunColor = Math::Vec3(0.3f, 0.3f, 0.4f);
        m_SunIntensity = 0.1f;
    }
}

void Skybox::SetSunDirection(const Math::Vec3& direction) {
    m_SunDirection = direction.Normalized();
}

void Skybox::SetSunColor(const Math::Vec3& color) {
    m_SunColor = color;
}

void Skybox::SetSunIntensity(float intensity) {
    m_SunIntensity = std::max(0.0f, intensity);
}

void Skybox::SetSunSize(float size) {
    m_SunSize = std::max(0.001f, std::min(0.5f, size));
}

void Skybox::SetHorizonColor(const Math::Vec3& color) {
    m_HorizonColor = color;
}

void Skybox::SetZenithColor(const Math::Vec3& color) {
    m_ZenithColor = color;
}

void Skybox::SetCloudDensity(float density) {
    m_CloudDensity = std::max(0.0f, std::min(1.0f, density));
}

void Skybox::SetAtmosphereThickness(float thickness) {
    m_AtmosphereThickness = std::max(0.1f, std::min(5.0f, thickness));
}

void Skybox::SetPreset(SkyPreset preset) {
    switch (preset) {
        case SkyPreset::Dawn:
            SetTimeOfDay(6.0f);
            m_CloudDensity = 0.2f;
            m_AtmosphereThickness = 1.2f;
            break;
        case SkyPreset::Noon:
            SetTimeOfDay(12.0f);
            m_CloudDensity = 0.1f;
            m_AtmosphereThickness = 1.0f;
            break;
        case SkyPreset::Dusk:
            SetTimeOfDay(18.0f);
            m_CloudDensity = 0.3f;
            m_AtmosphereThickness = 1.2f;
            break;
        case SkyPreset::Night:
            SetTimeOfDay(0.0f);
            m_CloudDensity = 0.5f;
            m_AtmosphereThickness = 0.8f;
            break;
        case SkyPreset::Overcast:
            m_HorizonColor = Math::Vec3(0.55f, 0.58f, 0.6f);
            m_ZenithColor = Math::Vec3(0.25f, 0.27f, 0.3f);
            m_SunColor = Math::Vec3(0.7f, 0.7f, 0.75f);
            m_SunIntensity = 0.1f;
            m_SunSize = 0.02f;
            m_CloudDensity = 0.8f;
            m_AtmosphereThickness = 0.5f;
            m_SunDirection = Math::Vec3(0.5f, 1.0f, -0.5f).Normalized();
            break;
        case SkyPreset::Clear:
            SetTimeOfDay(12.0f);
            m_CloudDensity = 0.0f;
            m_AtmosphereThickness = 1.5f;
            break;
    }
}

void Skybox::Regenerate() {
    if (!m_Initialized || m_ImageCubemapActive) {
        return;
    }

    // Regenerate cubemap faces with current parameters
    std::vector<std::vector<uint8_t>> facePixels(6);
    for (uint32_t face = 0; face < 6; face++) {
        facePixels[face].resize(m_Resolution * m_Resolution * 4);
        GenerateCubemapFace(face, m_Resolution, facePixels[face]);
    }

    // Update each cubemap face
    for (uint32_t face = 0; face < 6; face++) {
        const bgfx::Memory* mem = bgfx::copy(facePixels[face].data(), facePixels[face].size());
        bgfx::updateTextureCube(
            m_CubemapTexture,
            0,
            static_cast<uint8_t>(face),
            0,
            0,
            0,
            static_cast<uint16_t>(m_Resolution),
            static_cast<uint16_t>(m_Resolution),
            mem
        );
    }

    SIMPLE_LOG("Skybox: Regenerated cubemap with new parameters");
}

bool Skybox::LoadImageCubemapFromFacePaths(const std::array<std::string, 6>& paths, float brightness, std::string* errOut) {
    for (const std::string& p : paths) {
        if (p.empty()) {
            if (errOut) {
                *errOut = "Skybox: empty face path in cubemap set";
            }
            return false;
        }
    }

    std::array<std::vector<uint8_t>, 6> faceRgba{};
    std::array<uint32_t, 6> faceW{};
    std::array<uint32_t, 6> faceH{};
    int minSide = 4096;
    for (int i = 0; i < 6; ++i) {
        std::string err;
        if (!LoadImageFileToRgba8(paths[static_cast<size_t>(i)], faceRgba[static_cast<size_t>(i)], faceW[static_cast<size_t>(i)],
                faceH[static_cast<size_t>(i)], &err)) {
            if (errOut) {
                *errOut = err;
            }
            return false;
        }
        const int s = static_cast<int>(std::min(faceW[static_cast<size_t>(i)], faceH[static_cast<size_t>(i)]));
        minSide = std::min(minSide, s);
    }
    uint32_t faceSize = static_cast<uint32_t>(std::clamp(minSide, 64, 512));
    const float br = std::max(0.0f, brightness);

    std::array<std::vector<uint8_t>, 6> resized{};
    for (int i = 0; i < 6; ++i) {
        BlitResizeRgbaNearest(faceRgba[static_cast<size_t>(i)].data(), static_cast<int>(faceW[static_cast<size_t>(i)]),
            static_cast<int>(faceH[static_cast<size_t>(i)]), resized[static_cast<size_t>(i)], static_cast<int>(faceSize),
            static_cast<int>(faceSize));
        for (size_t j = 0; j + 3 < resized[static_cast<size_t>(i)].size(); j += 4) {
            resized[static_cast<size_t>(i)][j + 0] =
                static_cast<uint8_t>(std::clamp(std::round(static_cast<float>(resized[static_cast<size_t>(i)][j + 0]) * br), 0.0f, 255.0f));
            resized[static_cast<size_t>(i)][j + 1] =
                static_cast<uint8_t>(std::clamp(std::round(static_cast<float>(resized[static_cast<size_t>(i)][j + 1]) * br), 0.0f, 255.0f));
            resized[static_cast<size_t>(i)][j + 2] =
                static_cast<uint8_t>(std::clamp(std::round(static_cast<float>(resized[static_cast<size_t>(i)][j + 2]) * br), 0.0f, 255.0f));
        }
    }

    if (bgfx::isValid(m_CubemapTexture)) {
        bgfx::destroy(m_CubemapTexture);
        m_CubemapTexture = BGFX_INVALID_HANDLE;
    }

    m_CubemapTexture = bgfx::createTextureCube(static_cast<uint16_t>(faceSize), false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, nullptr);
    if (!bgfx::isValid(m_CubemapTexture)) {
        if (errOut) {
            *errOut = "Skybox: bgfx::createTextureCube failed";
        }
        return false;
    }

    for (uint32_t face = 0; face < 6; face++) {
        const bgfx::Memory* mem = bgfx::copy(resized[face].data(), static_cast<uint32_t>(resized[face].size()));
        bgfx::updateTextureCube(m_CubemapTexture, 0, static_cast<uint8_t>(face), 0, 0, 0,
            static_cast<uint16_t>(faceSize), static_cast<uint16_t>(faceSize), mem);
    }

    if (!m_SkyboxMesh) {
        CreateSkyboxMesh();
    }

    m_Resolution = faceSize;
    m_ImageCubemapActive = true;
    m_Initialized = true;
    SIMPLE_LOG("Skybox: Loaded image cubemap " + std::to_string(faceSize) + "px");
    return true;
}

void Skybox::ClearImageCubemapAndRegenerateProcedural(uint32_t proceduralResolution) {
    m_ImageCubemapActive = false;
    m_AuthoringImageYawDegrees = 0.f;
    m_AppliedAuthoringRevision = 0;
    Initialize(proceduralResolution);
}

} // namespace Solstice::Render
