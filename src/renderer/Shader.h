#pragma once

#include <glm/glm.hpp>
#include <string>

namespace lol
{
class Shader
{
public:
    Shader() = default;
    Shader(const std::string& vertexSource, const std::string& fragmentSource);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void Bind() const;
    unsigned int Id() const { return m_program; }
    void SetMat4(const std::string& name, const glm::mat4& value) const;
    void SetVec3(const std::string& name, const glm::vec3& value) const;
    void SetInt(const std::string& name, int value) const;

private:
    unsigned int m_program = 0;
};
}
