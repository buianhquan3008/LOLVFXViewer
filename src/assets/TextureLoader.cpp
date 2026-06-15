#include "assets/TextureLoader.h"

namespace lol
{
std::optional<Texture2D> TextureLoader::Load(const std::filesystem::path& path)
{
    return Texture2D::LoadFromFile(path);
}
}
