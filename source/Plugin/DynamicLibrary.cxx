#include <Plugin/DynamicLibrary.hxx>

namespace Solstice::Plugin {

DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept : m_Impl(std::move(other.m_Impl)) {}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept {
    if (this != &other) {
        m_Impl = std::move(other.m_Impl);
    }
    return *this;
}

DynamicLibrary::~DynamicLibrary() = default;

bool DynamicLibrary::Load(const std::string& pathUtf8, std::string* outError) {
    return m_Impl.Load(pathUtf8, outError);
}

void DynamicLibrary::Unload() {
    m_Impl.Unload();
}

void* DynamicLibrary::GetSymbol(const char* symbolName) const {
    return m_Impl.GetSymbol(symbolName);
}

} // namespace Solstice::Plugin
