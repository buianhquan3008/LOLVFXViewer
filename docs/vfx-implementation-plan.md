# Kế Hoạch Triển Khai VFX Viewer

Tài liệu này phân tích cách phát triển tính năng load và preview VFX cho project hiện tại. Mục tiêu là đi từ asset Riot thô tới một effect có thể play trong viewport, có texture, particle, blend mode và debug UI cơ bản.

Trạng thái hiện tại:

- Project đã có app shell, asset browser, viewport OpenGL, texture preview, model loader, skeleton, animation và CPU skinning.
- VFX mới có struct placeholder trong `src/vfx/VfxGraph.h`.
- Chưa có `VfxLoader`, chưa có parser BIN/VFX, chưa có particle runtime, chưa có VFX render pass, chưa có VFX panel.

Điểm quan trọng: VFX không nên nhét vào `ModelData`. Model và VFX có runtime khác nhau. Nên tạo pipeline riêng:

```text
Riot BIN/VFX files
-> VfxLoader
-> VfxGraph
-> VfxRuntime / ParticleSystem
-> Renderer::SetVfx / Renderer::RenderVfx
-> Viewport + VfxPanel
```

## 1. Cần Đọc Thêm Gì Từ File Riot

VFX của League thường không chỉ là một file mesh. Nó là một graph effect gồm emitter, material, texture, curve, timeline và child effect. Vì vậy loader cần gom thông tin từ nhiều nguồn.

### 1.1 Effect Definition

Thông tin cần parse:

- Tên effect.
- Danh sách emitter.
- Danh sách material.
- Danh sách texture reference.
- Danh sách curve/timeline.
- Danh sách child effect.
- Thời lượng effect.
- Loop hay one-shot.
- Local space hay world space.
- Attach point nếu effect bám vào model/bone.

Struct hiện có:

```cpp
struct VfxGraph
{
    std::string name;
    std::vector<VfxEmitter> emitters;
    std::vector<VfxMaterialRef> materials;
    std::vector<VfxCurve> curves;
    std::vector<VfxChildEffect> children;
};
```

Struct này đủ cho placeholder, nhưng chưa đủ để preview VFX thật. Cần mở rộng dần.

### 1.2 Emitter Data

Emitter là nơi sinh particle.

Tối thiểu cần đọc:

- `name`
- `spawnRate`
- `burstCount`
- `duration`
- `lifetimeMin`
- `lifetimeMax`
- `position`
- `rotation`
- `scale`
- `spawnShape`
- `velocityMin`
- `velocityMax`
- `acceleration`
- `drag`
- `startColor`
- `endColor`
- `startSize`
- `endSize`
- `texturePath`
- `materialName`
- `blendMode`
- `billboardMode`

`VfxEmitter` hiện tại đã có một số field cơ bản:

```cpp
struct VfxEmitter
{
    std::string name;
    float spawnRate = 0.0f;
    float lifetimeMin = 1.0f;
    float lifetimeMax = 1.0f;
    glm::vec3 velocityMin = glm::vec3(0.0f);
    glm::vec3 velocityMax = glm::vec3(0.0f);
    glm::vec4 startColor = glm::vec4(1.0f);
    glm::vec4 endColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    float startSize = 1.0f;
    float endSize = 0.0f;
    std::string texturePath;
    std::string materialName;
};
```

Nên bổ sung:

```cpp
enum class VfxBlendMode
{
    Alpha,
    Additive,
    Multiply
};

enum class VfxBillboardMode
{
    CameraFacing,
    VelocityFacing,
    WorldAligned
};

enum class VfxSpawnShape
{
    Point,
    Box,
    Sphere,
    Cone,
    Line
};

struct VfxEmitter
{
    std::string name;
    float spawnRate = 0.0f;
    int burstCount = 0;
    float durationSeconds = 1.0f;
    bool loop = true;

    float lifetimeMin = 1.0f;
    float lifetimeMax = 1.0f;

    VfxSpawnShape spawnShape = VfxSpawnShape::Point;
    glm::vec3 shapeExtents = glm::vec3(0.0f);

    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    glm::vec3 velocityMin = glm::vec3(0.0f);
    glm::vec3 velocityMax = glm::vec3(0.0f);
    glm::vec3 acceleration = glm::vec3(0.0f);
    float drag = 0.0f;

    glm::vec4 startColor = glm::vec4(1.0f);
    glm::vec4 endColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    float startSize = 1.0f;
    float endSize = 0.0f;

    std::string texturePath;
    std::string materialName;
    VfxBlendMode blendMode = VfxBlendMode::Additive;
    VfxBillboardMode billboardMode = VfxBillboardMode::CameraFacing;
};
```

### 1.3 Material Và Texture

Model path hiện mới load texture độc lập bằng `Texture2D::LoadFromFile`. VFX cần resolver riêng vì effect thường chỉ lưu reference.

Cần đọc:

- Tên material.
- Đường dẫn texture diffuse/main.
- Blend mode.
- Cull mode.
- Depth test.
- Depth write.
- UV scroll.
- Flipbook rows/columns/fps.
- Shader flags nếu có.

MVP có thể bắt đầu với:

```cpp
struct VfxMaterialRef
{
    std::string name;
    std::string texturePath;
    VfxBlendMode blendMode = VfxBlendMode::Additive;
    bool depthWrite = false;
    bool depthTest = true;
    int flipbookRows = 1;
    int flipbookColumns = 1;
    float flipbookFps = 0.0f;
};
```

### 1.4 Curves

VFX thật thường dùng curve để điều khiển size, alpha, color, velocity hoặc spawn rate theo thời gian.

Hiện có:

```cpp
struct VfxCurve
{
    std::string name;
    std::vector<glm::vec2> points;
};
```

Nên thêm loại curve:

```cpp
enum class VfxCurveTarget
{
    Size,
    Alpha,
    ColorR,
    ColorG,
    ColorB,
    Velocity,
    SpawnRate
};

struct VfxCurve
{
    std::string name;
    VfxCurveTarget target = VfxCurveTarget::Size;
    std::vector<glm::vec2> points;
};
```

Hàm sample curve:

```cpp
float SampleCurve(const VfxCurve& curve, float normalizedAge)
{
    if (curve.points.empty())
    {
        return 1.0f;
    }

    if (normalizedAge <= curve.points.front().x)
    {
        return curve.points.front().y;
    }

    if (normalizedAge >= curve.points.back().x)
    {
        return curve.points.back().y;
    }

    for (std::size_t i = 0; i + 1 < curve.points.size(); ++i)
    {
        const glm::vec2 a = curve.points[i];
        const glm::vec2 b = curve.points[i + 1];
        if (normalizedAge >= a.x && normalizedAge <= b.x)
        {
            const float t = (normalizedAge - a.x) / std::max(b.x - a.x, 0.0001f);
            return glm::mix(a.y, b.y, t);
        }
    }

    return curve.points.back().y;
}
```

### 1.5 Attachment

Muốn preview effect trên champion/model thì cần biết effect bám vào đâu.

Cần hỗ trợ:

- World space effect.
- Local space effect.
- Attach vào bone name.
- Attach vào transform thủ công.

Struct đề xuất:

```cpp
struct VfxAttachment
{
    bool attachToBone = false;
    std::string boneName;
    glm::mat4 localOffset = glm::mat4(1.0f);
};
```

Khi attach vào model đang animation:

```cpp
glm::mat4 ResolveVfxTransform(
    const Skeleton& skeleton,
    const std::vector<glm::mat4>& globalPose,
    const VfxAttachment& attachment)
{
    if (!attachment.attachToBone)
    {
        return attachment.localOffset;
    }

    const int boneIndex = skeleton.FindBoneIndex(attachment.boneName);
    if (boneIndex < 0 || boneIndex >= static_cast<int>(globalPose.size()))
    {
        return attachment.localOffset;
    }

    return globalPose[boneIndex] * attachment.localOffset;
}
```

## 2. File Và Class Nên Thêm

Nên giữ kiến trúc giống model loader hiện tại: loader parse file thành data nội bộ, runtime update data đó, renderer chỉ vẽ.

### 2.1 Files Trong `src/vfx`

Tạo mới:

```text
src/vfx/VfxGraph.h
src/vfx/VfxLoader.h
src/vfx/VfxLoader.cpp
src/vfx/VfxRuntime.h
src/vfx/VfxRuntime.cpp
src/vfx/Particle.h
src/vfx/VfxAssetResolver.h
src/vfx/VfxAssetResolver.cpp
```

Vai trò:

- `VfxGraph.h`: contract dữ liệu effect đã parse.
- `VfxLoader`: đọc file Riot và tạo `VfxGraph`.
- `VfxRuntime`: update emitter, spawn particle, kill particle.
- `Particle.h`: dữ liệu particle sống ở runtime.
- `VfxAssetResolver`: resolve texture/material/child effect path.

### 2.2 Files Trong `src/assets`

Sửa:

```text
src/assets/AssetManager.h
src/assets/AssetManager.cpp
```

Mục tiêu:

- Thêm `AssetSelectionType::Vfx`.
- Thêm `std::optional<VfxGraph> vfx`.
- Thêm `IsSupportedVfx`.
- Route file VFX sang `VfxLoader::Load`.

Code mẫu:

```cpp
enum class AssetSelectionType
{
    None,
    Texture,
    Model,
    Vfx,
    Data,
    Unsupported
};

struct AssetSelection
{
    AssetSelectionType type = AssetSelectionType::None;
    std::filesystem::path path;
    std::optional<Texture2D> texture;
    std::optional<ModelData> model;
    std::optional<VfxGraph> vfx;
    std::string metadata;
};
```

Trong `AssetManager`:

```cpp
bool AssetManager::IsSupportedVfx(const std::filesystem::path& path) const
{
    const auto ext = Lowercase(path.extension().string());
    const auto filename = Lowercase(path.filename().string());
    return ext == ".bin" || filename.ends_with(".bin.txt");
}
```

Route:

```cpp
if (IsSupportedVfx(path))
{
    selection.type = AssetSelectionType::Vfx;
    std::string errorMessage;
    selection.vfx = VfxLoader::Load(path, errorMessage);
    if (!selection.vfx)
    {
        selection.type = AssetSelectionType::Unsupported;
        selection.metadata = errorMessage;
    }
    return selection;
}
```

Ghi chú: extension thật cần xác nhận bằng sample Riot đang dùng. Ban đầu có thể support `.bin.txt` trước vì dễ debug hơn binary.

### 2.3 Files Trong `src/renderer`

Sửa:

```text
src/renderer/Renderer.h
src/renderer/Renderer.cpp
```

Thêm:

```text
src/renderer/VfxGpuResources.h
```

Mục tiêu:

- Tạo particle VAO/VBO riêng.
- Tạo shader riêng cho particle billboard.
- Thêm draw pass VFX sau model.

API đề xuất trong `Renderer`:

```cpp
void SetVfxParticles(const std::vector<VfxParticleRenderData>& particles);
void SetVfxTexture(unsigned int textureId);
void RenderVfx(const OrbitCamera& camera);
```

Hoặc gọn hơn:

```cpp
void SetVfxFrame(const VfxRenderFrame& frame);
```

### 2.4 Files Trong `src/ui`

Thêm:

```text
src/ui/VfxPanel.h
src/ui/VfxPanel.cpp
```

Sửa:

```text
src/ui/AppState.h
src/core/Application.h
src/core/Application.cpp
```

Mục tiêu UI:

- Play/pause/reset VFX.
- Hiển thị emitter count.
- Hiển thị particle count.
- Hiển thị texture/material resolve status.
- Cho phép bật/tắt từng emitter.
- Cho phép chỉnh playback speed.

## 3. VfxLoader: Parser Tối Thiểu

### 3.1 Public API

```cpp
#pragma once

#include "vfx/VfxGraph.h"

#include <filesystem>
#include <optional>
#include <string>

namespace lol
{
class VfxLoader
{
public:
    static std::optional<VfxGraph> Load(const std::filesystem::path& path, std::string& errorMessage);
};
}
```

### 3.2 Implementation Skeleton

```cpp
#include "vfx/VfxLoader.h"

#include "core/Log.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace lol
{
namespace
{
VfxGraph LoadDebugTextVfx(const std::filesystem::path& path)
{
    VfxGraph graph;
    graph.name = path.stem().string();

    VfxEmitter emitter;
    emitter.name = "DebugEmitter";
    emitter.spawnRate = 32.0f;
    emitter.lifetimeMin = 0.5f;
    emitter.lifetimeMax = 1.2f;
    emitter.velocityMin = glm::vec3(-0.5f, 1.0f, -0.5f);
    emitter.velocityMax = glm::vec3(0.5f, 2.0f, 0.5f);
    emitter.startColor = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f);
    emitter.endColor = glm::vec4(1.0f, 0.1f, 0.0f, 0.0f);
    emitter.startSize = 0.2f;
    emitter.endSize = 0.0f;

    graph.emitters.push_back(std::move(emitter));
    return graph;
}
}

std::optional<VfxGraph> VfxLoader::Load(const std::filesystem::path& path, std::string& errorMessage)
{
    try
    {
        VfxGraph graph = LoadDebugTextVfx(path);
        Log::Push(LogLevel::Info, "Loaded VFX graph: emitters=" + std::to_string(graph.emitters.size()), path.string());
        return graph;
    }
    catch (const std::exception& ex)
    {
        errorMessage = ex.what();
        return std::nullopt;
    }
}
}
```

Giai đoạn đầu có thể dùng loader giả lập như trên để phát triển runtime và renderer trước. Sau đó thay `LoadDebugTextVfx` bằng parser BIN thật.

## 4. VfxRuntime: Update Particle Mỗi Frame

### 4.1 Runtime Particle

```cpp
struct VfxParticle
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec4 color = glm::vec4(1.0f);
    float size = 1.0f;
    float rotation = 0.0f;
    float age = 0.0f;
    float lifetime = 1.0f;
    int emitterIndex = 0;
};
```

Render data nên tách khỏi runtime data:

```cpp
struct VfxParticleRenderData
{
    glm::vec3 position;
    glm::vec4 color;
    float size;
    float rotation;
    glm::vec4 uvRect;
};
```

### 4.2 VfxRuntime API

```cpp
class VfxRuntime
{
public:
    void SetGraph(const VfxGraph* graph);
    void Clear();
    void Restart();
    void SetPlaying(bool playing);
    void Update(float deltaSeconds);

    const std::vector<VfxParticleRenderData>& RenderParticles() const { return m_renderParticles; }
    int LiveParticleCount() const { return static_cast<int>(m_particles.size()); }

private:
    void SpawnFromEmitter(int emitterIndex, float deltaSeconds);
    void UpdateParticles(float deltaSeconds);
    void BuildRenderParticles();

    const VfxGraph* m_graph = nullptr;
    bool m_playing = true;
    float m_timeSeconds = 0.0f;
    std::vector<float> m_spawnAccumulators;
    std::vector<VfxParticle> m_particles;
    std::vector<VfxParticleRenderData> m_renderParticles;
};
```

### 4.3 Spawn Logic

```cpp
void VfxRuntime::SpawnFromEmitter(int emitterIndex, float deltaSeconds)
{
    const VfxEmitter& emitter = m_graph->emitters[emitterIndex];
    float& accumulator = m_spawnAccumulators[emitterIndex];

    accumulator += emitter.spawnRate * deltaSeconds;
    const int spawnCount = static_cast<int>(accumulator);
    accumulator -= static_cast<float>(spawnCount);

    for (int i = 0; i < spawnCount; ++i)
    {
        VfxParticle particle;
        particle.position = emitter.position;
        particle.velocity = RandomVec3(emitter.velocityMin, emitter.velocityMax);
        particle.lifetime = RandomFloat(emitter.lifetimeMin, emitter.lifetimeMax);
        particle.color = emitter.startColor;
        particle.size = emitter.startSize;
        particle.emitterIndex = emitterIndex;
        m_particles.push_back(particle);
    }
}
```

`RandomFloat` và `RandomVec3` nên là helper deterministic nếu sau này muốn replay/debug ổn định.

### 4.4 Update Logic

```cpp
void VfxRuntime::UpdateParticles(float deltaSeconds)
{
    for (VfxParticle& particle : m_particles)
    {
        const VfxEmitter& emitter = m_graph->emitters[particle.emitterIndex];

        particle.age += deltaSeconds;
        particle.velocity += emitter.acceleration * deltaSeconds;
        particle.velocity *= std::max(0.0f, 1.0f - emitter.drag * deltaSeconds);
        particle.position += particle.velocity * deltaSeconds;

        const float t = std::clamp(particle.age / std::max(particle.lifetime, 0.0001f), 0.0f, 1.0f);
        particle.color = glm::mix(emitter.startColor, emitter.endColor, t);
        particle.size = glm::mix(emitter.startSize, emitter.endSize, t);
    }

    std::erase_if(m_particles, [](const VfxParticle& particle) {
        return particle.age >= particle.lifetime;
    });
}
```

### 4.5 Build Render Data

```cpp
void VfxRuntime::BuildRenderParticles()
{
    m_renderParticles.clear();
    m_renderParticles.reserve(m_particles.size());

    for (const VfxParticle& particle : m_particles)
    {
        if (particle.size <= 0.0f || particle.color.a <= 0.0f)
        {
            continue;
        }

        m_renderParticles.push_back(VfxParticleRenderData{
            .position = particle.position,
            .color = particle.color,
            .size = particle.size,
            .rotation = particle.rotation,
            .uvRect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
        });
    }
}
```

## 5. Renderer: Particle Billboard Pass

Renderer hiện vẽ grid, model và skeleton. VFX nên render sau model vì VFX thường transparent/additive.

Thứ tự đề xuất trong `Renderer::Render`:

```text
1. Clear framebuffer.
2. Draw grid.
3. Draw opaque model.
4. Draw skeleton debug lines.
5. Draw VFX particles.
6. Unbind framebuffer.
```

### 5.1 VFX Vertex Data

Cách đơn giản: CPU expand mỗi particle thành 4 vertices + 6 indices.

```cpp
struct VfxBillboardVertex
{
    glm::vec3 position;
    glm::vec2 texCoord;
    glm::vec4 color;
};
```

GPU resource:

```cpp
struct GpuVfxBuffer
{
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    int indexCount = 0;
};
```

### 5.2 Build Billboard Vertices

```cpp
void BuildBillboards(
    const std::vector<VfxParticleRenderData>& particles,
    const OrbitCamera& camera,
    std::vector<VfxBillboardVertex>& vertices,
    std::vector<unsigned int>& indices)
{
    vertices.clear();
    indices.clear();

    const glm::vec3 right = camera.Right();
    const glm::vec3 up = camera.Up();

    for (const VfxParticleRenderData& particle : particles)
    {
        const unsigned int base = static_cast<unsigned int>(vertices.size());
        const float halfSize = particle.size * 0.5f;

        const glm::vec3 dx = right * halfSize;
        const glm::vec3 dy = up * halfSize;

        vertices.push_back({particle.position - dx - dy, glm::vec2(0.0f, 0.0f), particle.color});
        vertices.push_back({particle.position + dx - dy, glm::vec2(1.0f, 0.0f), particle.color});
        vertices.push_back({particle.position + dx + dy, glm::vec2(1.0f, 1.0f), particle.color});
        vertices.push_back({particle.position - dx + dy, glm::vec2(0.0f, 1.0f), particle.color});

        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }
}
```

Hiện `OrbitCamera` chưa chắc có `Right()` và `Up()`. Nếu chưa có, thêm helper:

```cpp
glm::vec3 OrbitCamera::Forward() const;
glm::vec3 OrbitCamera::Right() const;
glm::vec3 OrbitCamera::Up() const;
```

Hoặc tính từ view matrix trong renderer.

### 5.3 VFX Shader

Shader tối thiểu:

```glsl
#version 460 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec4 aColor;

uniform mat4 uViewProjection;

out vec2 vTexCoord;
out vec4 vColor;

void main()
{
    vTexCoord = aTexCoord;
    vColor = aColor;
    gl_Position = uViewProjection * vec4(aPosition, 1.0);
}
```

Fragment:

```glsl
#version 460 core
in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uTexture;
uniform int uUseTexture;

out vec4 FragColor;

void main()
{
    vec4 texel = uUseTexture != 0 ? texture(uTexture, vTexCoord) : vec4(1.0);
    FragColor = texel * vColor;
}
```

### 5.4 Blend State

Alpha blend:

```cpp
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
glDepthMask(GL_FALSE);
```

Additive:

```cpp
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE);
glDepthMask(GL_FALSE);
```

Sau khi vẽ VFX:

```cpp
glDepthMask(GL_TRUE);
glDisable(GL_BLEND);
```

MVP nên dùng additive trước vì nhiều effect skill/spell sẽ nhìn hợp lý hơn và ít cần sort hơn alpha blend.

## 6. Application Và AppState

`AppState` nên sở hữu runtime VFX giống cách nó đang sở hữu `Animator`.

Đề xuất:

```cpp
struct AppState
{
    AssetManager assetManager;
    AssetSelection selection;
    OrbitCamera camera;
    ViewportOptions viewportOptions;

    bool pauseAnimation = false;
    bool enableSkinning = true;
    Animator animator;

    bool pauseVfx = false;
    float vfxPlaybackSpeed = 1.0f;
    VfxRuntime vfxRuntime;
};
```

Trong `Application::Run`:

```cpp
if (m_state.selection.type == AssetSelectionType::Vfx && m_state.selection.vfx)
{
    m_state.vfxRuntime.SetPlaying(!m_state.pauseVfx);
    m_state.vfxRuntime.Update(m_timeStep.deltaSeconds * m_state.vfxPlaybackSpeed);
    m_renderer->SetVfxFrame(m_state.vfxRuntime.RenderParticles());
}
else
{
    m_renderer->SetVfxFrame({});
}
```

Cần thêm sync giống `SyncRendererSelection`, ví dụ:

```cpp
void Application::SyncVfxSelection()
{
    if (m_state.selection.type == AssetSelectionType::Vfx && m_state.selection.vfx)
    {
        if (m_lastRenderedVfxPath != m_state.selection.path)
        {
            m_state.vfxRuntime.SetGraph(&*m_state.selection.vfx);
            m_state.vfxRuntime.Restart();
            m_lastRenderedVfxPath = m_state.selection.path;
        }
        return;
    }

    if (!m_lastRenderedVfxPath.empty())
    {
        m_state.vfxRuntime.Clear();
        m_renderer->SetVfxFrame({});
        m_lastRenderedVfxPath.clear();
    }
}
```

## 7. UI Debug Cho VFX

Thêm `VfxPanel`, tương tự `AnimationPanel`.

Thông tin nên hiển thị:

- Tên graph.
- Số emitter.
- Số particle live.
- Play/pause/reset.
- Playback speed.
- Danh sách emitter.
- Texture/material của từng emitter.
- Warnings: missing texture, unsupported blend mode, unsupported curve.

API:

```cpp
class VfxPanel
{
public:
    void Draw(AppState& state);
};
```

Skeleton UI:

```cpp
void VfxPanel::Draw(AppState& state)
{
    ImGui::Begin("VFX");

    if (state.selection.type != AssetSelectionType::Vfx || !state.selection.vfx)
    {
        ImGui::TextUnformatted("Open a VFX asset.");
        ImGui::End();
        return;
    }

    const VfxGraph& graph = *state.selection.vfx;
    ImGui::Text("Graph: %s", graph.name.c_str());
    ImGui::Text("Emitters: %d", static_cast<int>(graph.emitters.size()));
    ImGui::Text("Live particles: %d", state.vfxRuntime.LiveParticleCount());

    if (ImGui::Button(state.pauseVfx ? "Play" : "Pause"))
    {
        state.pauseVfx = !state.pauseVfx;
    }

    ImGui::SameLine();
    if (ImGui::Button("Restart"))
    {
        state.vfxRuntime.Restart();
    }

    ImGui::SliderFloat("Speed", &state.vfxPlaybackSpeed, 0.1f, 3.0f, "%.2fx");

    for (const VfxEmitter& emitter : graph.emitters)
    {
        if (ImGui::TreeNode(emitter.name.c_str()))
        {
            ImGui::Text("Spawn Rate: %.2f", emitter.spawnRate);
            ImGui::Text("Lifetime: %.2f - %.2f", emitter.lifetimeMin, emitter.lifetimeMax);
            ImGui::Text("Texture: %s", emitter.texturePath.c_str());
            ImGui::Text("Material: %s", emitter.materialName.c_str());
            ImGui::TreePop();
        }
    }

    ImGui::End();
}
```

## 8. Asset Resolver

VFX thường lưu texture/material path tương đối hoặc hashed reference. Không nên để loader trực tiếp đoán mọi đường dẫn. Tạo resolver riêng.

```cpp
class VfxAssetResolver
{
public:
    explicit VfxAssetResolver(std::filesystem::path root);

    std::optional<std::filesystem::path> ResolveTexture(const std::string& textureRef) const;
    std::optional<std::filesystem::path> ResolveChildEffect(const std::string& effectRef) const;

private:
    std::filesystem::path m_root;
};
```

Logic ban đầu:

```cpp
std::optional<std::filesystem::path> VfxAssetResolver::ResolveTexture(const std::string& textureRef) const
{
    const std::filesystem::path direct = m_root / textureRef;
    if (std::filesystem::exists(direct))
    {
        return direct;
    }

    const std::filesystem::path png = m_root / (textureRef + ".png");
    if (std::filesystem::exists(png))
    {
        return png;
    }

    const std::filesystem::path tga = m_root / (textureRef + ".tga");
    if (std::filesystem::exists(tga))
    {
        return tga;
    }

    return std::nullopt;
}
```

Sau này nếu hỗ trợ `.tex`, resolver trả path `.tex`, rồi `TextureLoader` cần biết decode `.tex`.

## 9. Texture Cho VFX

Hiện `Texture2D` move-only, nên không nên copy texture vào `VfxGraph`. Renderer hoặc runtime nên có cache texture theo path.

Đề xuất:

```cpp
class TextureCache
{
public:
    unsigned int LoadOrGetTextureId(const std::filesystem::path& path);
    void Clear();

private:
    std::unordered_map<std::filesystem::path, Texture2D> m_textures;
};
```

Nếu muốn giữ scope nhỏ, cache có thể nằm trong `Renderer` trước:

```cpp
std::unordered_map<std::filesystem::path, Texture2D> m_vfxTextures;
```

MVP có thể bắt đầu bằng một texture trắng fallback:

```cpp
unsigned int m_whiteTexture = 0;
```

Khi texture thiếu, vẫn render particle bằng màu vertex để debug emitter.

## 10. Các Bước Triển Khai Cụ Thể

### Bước 1: Củng cố data model

File:

```text
src/vfx/VfxGraph.h
```

Làm:

- Thêm enum `VfxBlendMode`.
- Thêm enum `VfxBillboardMode`.
- Thêm enum `VfxSpawnShape`.
- Mở rộng `VfxEmitter`.
- Mở rộng `VfxMaterialRef`.
- Thêm `VfxAttachment` nếu muốn attach vào model sau này.

Kết quả mong muốn:

- Có contract đủ để runtime và renderer dùng mà không phụ thuộc format Riot raw.

### Bước 2: Thêm VfxLoader giả lập

File:

```text
src/vfx/VfxLoader.h
src/vfx/VfxLoader.cpp
```

Làm:

- Tạo `VfxLoader::Load`.
- Ban đầu có thể tạo graph giả từ file bất kỳ để test runtime.
- Sau đó mới thay bằng BIN parser thật.

Kết quả mong muốn:

- Click vào VFX asset có thể tạo `VfxGraph` với ít nhất 1 emitter.

### Bước 3: Route VFX trong AssetManager

File:

```text
src/assets/AssetManager.h
src/assets/AssetManager.cpp
src/ui/AssetBrowserPanel.cpp
```

Làm:

- Thêm `AssetSelectionType::Vfx`.
- Thêm `std::optional<VfxGraph> vfx`.
- Thêm `IsSupportedVfx`.
- Sửa supported check trong asset browser.
- Route file VFX sang `VfxLoader`.

Kết quả mong muốn:

- VFX xuất hiện là supported asset.
- `AssetSelection` giữ được `VfxGraph`.

### Bước 4: Thêm VfxRuntime

File:

```text
src/vfx/Particle.h
src/vfx/VfxRuntime.h
src/vfx/VfxRuntime.cpp
src/ui/AppState.h
```

Làm:

- Tạo runtime particle.
- Spawn particle theo `spawnRate`.
- Update position, color, size theo age.
- Kill particle hết lifetime.
- Build `VfxParticleRenderData`.

Kết quả mong muốn:

- Dù chưa render, debug panel thấy `Live particles` tăng/giảm đúng.

### Bước 5: Thêm VFX render pass

File:

```text
src/renderer/Renderer.h
src/renderer/Renderer.cpp
```

Làm:

- Thêm particle shader.
- Thêm VAO/VBO/EBO cho billboard.
- Thêm `SetVfxFrame`.
- Thêm `RenderVfx`.
- Bật blend mode khi vẽ VFX.

Kết quả mong muốn:

- Particle hiện trong viewport dưới dạng quad màu hoặc texture trắng.

### Bước 6: Thêm texture resolver/cache

File:

```text
src/vfx/VfxAssetResolver.h
src/vfx/VfxAssetResolver.cpp
src/renderer/Renderer.cpp
```

Làm:

- Resolve texture reference.
- Load texture bằng `Texture2D::LoadFromFile`.
- Cache texture theo path.
- Dùng fallback white texture nếu thiếu.

Kết quả mong muốn:

- Particle render bằng texture thật khi resolve được.

### Bước 7: Thêm VfxPanel

File:

```text
src/ui/VfxPanel.h
src/ui/VfxPanel.cpp
src/core/Application.h
src/core/Application.cpp
```

Làm:

- Thêm panel vào app.
- Play/pause/restart.
- Hiển thị emitter/material/texture.
- Hiển thị warnings.

Kết quả mong muốn:

- Debug được effect mà không cần log quá nhiều.

### Bước 8: Parser Riot thật

File:

```text
src/vfx/VfxLoader.cpp
src/bin/BinParser.h
```

Làm:

- Chọn input format đầu tiên: `.bin.txt` hoặc `.bin`.
- Nếu `.bin.txt`: parse text trước để dễ debug.
- Nếu `.bin`: cần binary parser có schema rõ ràng.
- Map field Riot sang `VfxGraph`.

Kết quả mong muốn:

- Load được effect sample thật.
- Những field chưa hỗ trợ vẫn log warning thay vì crash.

## 11. Những Gì Chưa Nên Làm Ngay

Không nên làm ngay trong MVP:

- GPU particle simulation.
- Compute shader.
- Full material system.
- Distortion/refraction.
- Soft particles.
- Ribbon/trail phức tạp.
- Exact parity với render trong game.
- Parser mọi biến thể BIN ngay từ đầu.

Nên làm bản nhỏ trước:

```text
1 emitter
-> spawn particles
-> billboard camera-facing
-> additive blend
-> texture fallback
-> debug UI
```

Sau khi MVP chạy ổn mới mở rộng.

## 12. Definition Of Done Cho VFX MVP

Tính năng VFX MVP nên được coi là xong khi:

- Asset browser nhận diện được một file VFX.
- `VfxLoader` trả về `VfxGraph` có ít nhất một emitter.
- Runtime spawn và update particle ổn định.
- Renderer vẽ particle billboard trong viewport.
- Particle có blend mode additive hoặc alpha.
- Có fallback texture để effect vẫn hiện khi thiếu texture.
- VfxPanel hiển thị emitter count, live particle count, play/pause/restart.
- Missing texture/material/unsupported field được log rõ ràng.
- App không crash khi gặp field Riot chưa hỗ trợ.

## 13. Rủi Ro Kỹ Thuật

### Format Riot chưa chắc ổn định

Các file Riot có thể khác layout giữa phiên bản game. Parser cần log version/type/hash thay vì fail im lặng.

### Texture `.tex`

Project hiện chỉ load `.png`, `.jpg`, `.jpeg`, `.tga`. Nếu asset Riot dùng `.tex`, cần thêm `RiotTexLoader` hoặc bước convert ngoài.

### Blend và sort

Alpha particle cần sort back-to-front để nhìn đúng. MVP có thể ưu tiên additive để giảm yêu cầu sort.

### Coordinate convention

VFX có thể dùng local/world coordinate khác model. Cần debug transform bằng gizmo hoặc emitter origin line.

### Attachment vào bone

Attach vào animated bone cần map bone name chính xác và lấy transform từ `Animator::GlobalPose`.

### Performance

CPU billboard expansion ổn cho MVP. Khi particle nhiều, cần chuyển sang instancing hoặc GPU simulation.

## 14. Roadmap Ngắn Gọn

Roadmap thực tế:

```text
Milestone 1: VfxGraph + VfxLoader giả lập
Milestone 2: AssetManager route VFX
Milestone 3: VfxRuntime CPU particle
Milestone 4: Renderer billboard pass + fallback texture
Milestone 5: VfxPanel debug
Milestone 6: Parse `.bin.txt` hoặc sample Riot thật
Milestone 7: Resolve texture/material thật
Milestone 8: Attach VFX vào model/bone
Milestone 9: Curves, flipbook, child effects
Milestone 10: Ribbons/trails/distortion nếu cần
```

Với project hiện tại, nên bắt đầu từ Milestone 1 đến 5 trước. Đây là đường ngắn nhất để nhìn thấy particle trong viewport và có nền để debug parser Riot thật.
