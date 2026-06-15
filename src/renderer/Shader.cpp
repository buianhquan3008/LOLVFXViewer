#include "renderer/Shader.h"

#include <glad/gl.h>
#include <stdexcept>
#include <vector>

namespace lol
{
namespace
{
unsigned int Compile(unsigned int type, const std::string& source)
{
    const char* rawSource = source.c_str();
    const auto shader = glCreateShader(type);
    glShaderSource(shader, 1, &rawSource, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE)
    {
        int length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> infoLog(static_cast<std::size_t>(length));
        glGetShaderInfoLog(shader, length, nullptr, infoLog.data());
        glDeleteShader(shader);
        throw std::runtime_error(infoLog.data());
    }

    return shader;
}
}

Shader::Shader(const std::string& vertexSource, const std::string& fragmentSource)
{
    const auto vertex = Compile(GL_VERTEX_SHADER, vertexSource);
    const auto fragment = Compile(GL_FRAGMENT_SHADER, fragmentSource);

    m_program = glCreateProgram();
    glAttachShader(m_program, vertex);
    glAttachShader(m_program, fragment);
    glLinkProgram(m_program);

    int success = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (success == GL_FALSE)
    {
        int length = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> infoLog(static_cast<std::size_t>(length));
        glGetProgramInfoLog(m_program, length, nullptr, infoLog.data());
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        glDeleteProgram(m_program);
        m_program = 0;
        throw std::runtime_error(infoLog.data());
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

Shader::~Shader()
{
    if (m_program != 0)
    {
        glDeleteProgram(m_program);
    }
}

Shader::Shader(Shader&& other) noexcept
{
    m_program = other.m_program;
    other.m_program = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept
{
    if (this != &other)
    {
        if (m_program != 0)
        {
            glDeleteProgram(m_program);
        }
        m_program = other.m_program;
        other.m_program = 0;
    }
    return *this;
}

void Shader::Bind() const
{
    glUseProgram(m_program);
}

void Shader::SetMat4(const std::string& name, const glm::mat4& value) const
{
    glUniformMatrix4fv(glGetUniformLocation(m_program, name.c_str()), 1, GL_FALSE, &value[0][0]);
}

void Shader::SetVec3(const std::string& name, const glm::vec3& value) const
{
    glUniform3fv(glGetUniformLocation(m_program, name.c_str()), 1, &value[0]);
}

void Shader::SetInt(const std::string& name, int value) const
{
    glUniform1i(glGetUniformLocation(m_program, name.c_str()), value);
}
}
