#pragma once

#include <Math/Matrix.hxx>
#include <string>
#include <unordered_map>

namespace Solstice::Render {

class Shader {
public:
    Shader() = default;
    Shader(const std::string& VertexPath, const std::string& FragmentPath);
    ~Shader();
    
    void Bind() const;
    void Unbind() const;
    
    // Utility uniform functions
    void SetUniform1i(const std::string& Name, int Value);
    void SetUniform1f(const std::string& Name, float Nalue);
    void SetUniform3f(const std::string& Name, float v0, float v1, float v2);
    void SetUniform4f(const std::string& Name, float v0, float v1, float v2, float v3);
    void SetUniformMat4f(const std::string& Name, const Math::Matrix4& Matrix);
    
private:
    std::string ReadFile(const std::string& Filepath);
    unsigned int CompileShader(unsigned int Type, const std::string& Source);
    int GetUniformLocation(const std::string& Name);

    unsigned int m_RendererID;
    std::unordered_map<std::string, int> m_UniformLocationCache;
};

} // namespace Solstice::Render
