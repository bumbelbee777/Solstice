#include <UI/Media/VideoTexture.hxx>

namespace Solstice::UI {

VideoTexture::~VideoTexture() {
    Close();
}

void VideoTexture::Close() {
    m_Decoder.Close();
    if (bgfx::isValid(m_Texture)) {
        bgfx::destroy(m_Texture);
        m_Texture = BGFX_INVALID_HANDLE;
    }
    m_AllocW = 0;
    m_AllocH = 0;
}

bool VideoTexture::Open(const std::string& Path, std::string& Err) {
    Close();
    if (!m_Decoder.Open(Path, Err)) {
        return false;
    }
    std::string derr;
    if (!m_Decoder.DecodeNextFrame(derr)) {
        Err = derr.empty() ? "no video frame" : derr;
        m_Decoder.Close();
        return false;
    }
    EnsureTexture(m_Decoder.Width(), m_Decoder.Height());
    if (!bgfx::isValid(m_Texture)) {
        Err = "bgfx::createTexture2D failed";
        m_Decoder.Close();
        return false;
    }
    const bgfx::Memory* mem = bgfx::copy(
        m_Decoder.RgbaData(),
        static_cast<uint32_t>(m_Decoder.Width() * m_Decoder.Height() * 4u));
    bgfx::updateTexture2D(
        m_Texture,
        0,
        0,
        0,
        0,
        static_cast<uint16_t>(m_Decoder.Width()),
        static_cast<uint16_t>(m_Decoder.Height()),
        mem,
        static_cast<uint16_t>(m_Decoder.RgbaStrideBytes()));
    return true;
}

void VideoTexture::EnsureTexture(uint32_t W, uint32_t H) {
    if (W == 0 || H == 0) {
        return;
    }
    if (bgfx::isValid(m_Texture) && m_AllocW == W && m_AllocH == H) {
        return;
    }
    if (bgfx::isValid(m_Texture)) {
        bgfx::destroy(m_Texture);
        m_Texture = BGFX_INVALID_HANDLE;
    }
    m_Texture = bgfx::createTexture2D(
        static_cast<uint16_t>(W),
        static_cast<uint16_t>(H),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_NONE,
        nullptr);
    if (bgfx::isValid(m_Texture)) {
        bgfx::setName(m_Texture, "UI Video");
        m_AllocW = W;
        m_AllocH = H;
    } else {
        m_AllocW = 0;
        m_AllocH = 0;
    }
}

bool VideoTexture::StepFrame(std::string& Err) {
    Err.clear();
    if (!IsOpen()) {
        Err = "VideoTexture not open";
        return false;
    }
    if (!m_Decoder.DecodeNextFrame(Err)) {
        return false;
    }
    EnsureTexture(m_Decoder.Width(), m_Decoder.Height());
    if (!bgfx::isValid(m_Texture)) {
        Err = "texture resize failed";
        return false;
    }
    const bgfx::Memory* mem = bgfx::copy(
        m_Decoder.RgbaData(),
        static_cast<uint32_t>(m_Decoder.Width() * m_Decoder.Height() * 4u));
    bgfx::updateTexture2D(
        m_Texture,
        0,
        0,
        0,
        0,
        static_cast<uint16_t>(m_Decoder.Width()),
        static_cast<uint16_t>(m_Decoder.Height()),
        mem,
        static_cast<uint16_t>(m_Decoder.RgbaStrideBytes()));
    return true;
}

} // namespace Solstice::UI
