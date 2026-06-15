#pragma once

#include <filesystem>
#include <optional>

namespace lol
{
struct TextureInfo
{
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned int rendererId = 0;
    std::filesystem::path path;
};

class Texture2D
{
public:
    Texture2D() = default;
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;
    Texture2D(Texture2D&& other) noexcept;
    Texture2D& operator=(Texture2D&& other) noexcept;

    static std::optional<Texture2D> LoadFromFile(const std::filesystem::path& path);

    unsigned int Id() const { return m_info.rendererId; }
    const TextureInfo& Info() const { return m_info; }

private:
    TextureInfo m_info;
};
}
