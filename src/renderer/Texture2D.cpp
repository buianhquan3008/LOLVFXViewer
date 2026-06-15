#include "renderer/Texture2D.h"

#include <glad/gl.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace lol
{
Texture2D::~Texture2D()
{
    if (m_info.rendererId != 0)
    {
        glDeleteTextures(1, &m_info.rendererId);
    }
}

Texture2D::Texture2D(Texture2D&& other) noexcept
{
    m_info = other.m_info;
    other.m_info = {};
}

Texture2D& Texture2D::operator=(Texture2D&& other) noexcept
{
    if (this != &other)
    {
        if (m_info.rendererId != 0)
        {
            glDeleteTextures(1, &m_info.rendererId);
        }
        m_info = other.m_info;
        other.m_info = {};
    }
    return *this;
}

std::optional<Texture2D> Texture2D::LoadFromFile(const std::filesystem::path& path)
{
    stbi_set_flip_vertically_on_load(1);
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load(path.string().c_str(), &width, &height, &channels, 0);
    if (data == nullptr)
    {
        return std::nullopt;
    }

    GLenum format = GL_RGB;
    if (channels == 1)
    {
        format = GL_RED;
    }
    else if (channels == 4)
    {
        format = GL_RGBA;
    }

    Texture2D texture;
    texture.m_info.path = path;
    texture.m_info.width = width;
    texture.m_info.height = height;
    texture.m_info.channels = channels;

    GLenum internalFormat = GL_RGB8;
    if (channels == 1)
    {
        internalFormat = GL_R8;
    }
    else if (channels == 4)
    {
        internalFormat = GL_RGBA8;
    }

    glCreateTextures(GL_TEXTURE_2D, 1, &texture.m_info.rendererId);
    glTextureStorage2D(texture.m_info.rendererId, 1, internalFormat, width, height);
    glTextureParameteri(texture.m_info.rendererId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture.m_info.rendererId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(texture.m_info.rendererId, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(texture.m_info.rendererId, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureSubImage2D(texture.m_info.rendererId, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    return texture;
}
}
