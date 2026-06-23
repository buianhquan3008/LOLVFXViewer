#include "renderer/Renderer.h"

#include "assets/TextureLoader.h"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstddef>
#include <stdexcept>

namespace lol
{
namespace
{
constexpr const char* kStaticModelVertexShader = R"(
#version 460 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uViewProjection;
uniform mat4 uModel;

out vec3 vNormal;
out vec3 vWorldPosition;

void main()
{
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorldPosition = world.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uViewProjection * world;
}
)";

constexpr const char* kModelFragmentShader = R"(
#version 460 core
in vec3 vNormal;
in vec3 vWorldPosition;

uniform vec3 uCameraPosition;

out vec4 FragColor;

void main()
{
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.4, 0.9, 0.2));
    float diffuse = max(dot(normal, lightDir), 0.0);
    vec3 baseColor = vec3(0.72, 0.78, 0.88);
    vec3 ambient = vec3(0.22);
    FragColor = vec4(baseColor * (ambient + diffuse), 1.0);
}
)";

constexpr const char* kGridVertexShader = R"(
#version 460 core
layout (location = 0) in vec3 aPosition;

uniform mat4 uViewProjection;

void main()
{
    gl_Position = uViewProjection * vec4(aPosition, 1.0);
}
)";

constexpr const char* kGridFragmentShader = R"(
#version 460 core
uniform vec3 uColor;
out vec4 FragColor;
void main()
{
    FragColor = vec4(uColor, 1.0);
}
)";

constexpr const char* kParticleVertexShader = R"(
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
)";

constexpr const char* kParticleFragmentShader = R"(
#version 460 core
in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uParticleTexture;

out vec4 FragColor;

void main()
{
    vec4 sampled = texture(uParticleTexture, vTexCoord);
    FragColor = sampled * vColor;
    if (FragColor.a <= 0.01)
    {
        discard;
    }
}
)";
}

Renderer::Renderer()
    : m_staticModelShader(kStaticModelVertexShader, kModelFragmentShader),
      m_gridShader(kGridVertexShader, kGridFragmentShader),
      m_particleShader(kParticleVertexShader, kParticleFragmentShader)
{
    RebuildFramebuffer();

    std::vector<glm::vec3> gridLines;
    constexpr int extent = 20;
    for (int i = -extent; i <= extent; ++i)
    {
        gridLines.emplace_back(static_cast<float>(i), 0.0f, static_cast<float>(-extent));
        gridLines.emplace_back(static_cast<float>(i), 0.0f, static_cast<float>(extent));
        gridLines.emplace_back(static_cast<float>(-extent), 0.0f, static_cast<float>(i));
        gridLines.emplace_back(static_cast<float>(extent), 0.0f, static_cast<float>(i));
    }

    m_gridVertexCount = static_cast<int>(gridLines.size());
    glCreateVertexArrays(1, &m_gridVao);
    glCreateBuffers(1, &m_gridVbo);
    glNamedBufferData(m_gridVbo, static_cast<GLsizeiptr>(gridLines.size() * sizeof(glm::vec3)), gridLines.data(), GL_STATIC_DRAW);
    glVertexArrayVertexBuffer(m_gridVao, 0, m_gridVbo, 0, sizeof(glm::vec3));
    glEnableVertexArrayAttrib(m_gridVao, 0);
    glVertexArrayAttribFormat(m_gridVao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(m_gridVao, 0, 0);

    glCreateVertexArrays(1, &m_skeletonVao);
    glCreateBuffers(1, &m_skeletonVbo);
    glVertexArrayVertexBuffer(m_skeletonVao, 0, m_skeletonVbo, 0, sizeof(glm::vec3));
    glEnableVertexArrayAttrib(m_skeletonVao, 0);
    glVertexArrayAttribFormat(m_skeletonVao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(m_skeletonVao, 0, 0);

    glCreateVertexArrays(1, &m_particleVao);
    glCreateBuffers(1, &m_particleVbo);
    glVertexArrayVertexBuffer(m_particleVao, 0, m_particleVbo, 0, sizeof(VfxVertex));
    glEnableVertexArrayAttrib(m_particleVao, 0);
    glEnableVertexArrayAttrib(m_particleVao, 1);
    glEnableVertexArrayAttrib(m_particleVao, 2);
    glVertexArrayAttribFormat(m_particleVao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(VfxVertex, position));
    glVertexArrayAttribFormat(m_particleVao, 1, 2, GL_FLOAT, GL_FALSE, offsetof(VfxVertex, uv));
    glVertexArrayAttribFormat(m_particleVao, 2, 4, GL_FLOAT, GL_FALSE, offsetof(VfxVertex, color));
    glVertexArrayAttribBinding(m_particleVao, 0, 0);
    glVertexArrayAttribBinding(m_particleVao, 1, 0);
    glVertexArrayAttribBinding(m_particleVao, 2, 0);

    CreateFallbackParticleTexture();
}

Renderer::~Renderer()
{
    ReleaseModel();
    if (m_defaultParticleTexture != 0) glDeleteTextures(1, &m_defaultParticleTexture);
    if (m_particleVbo != 0) glDeleteBuffers(1, &m_particleVbo);
    if (m_particleVao != 0) glDeleteVertexArrays(1, &m_particleVao);
    if (m_skeletonVbo != 0) glDeleteBuffers(1, &m_skeletonVbo);
    if (m_skeletonVao != 0) glDeleteVertexArrays(1, &m_skeletonVao);
    if (m_gridVbo != 0) glDeleteBuffers(1, &m_gridVbo);
    if (m_gridVao != 0) glDeleteVertexArrays(1, &m_gridVao);
    if (m_depthTexture != 0) glDeleteTextures(1, &m_depthTexture);
    if (m_colorTexture != 0) glDeleteTextures(1, &m_colorTexture);
    if (m_framebuffer != 0) glDeleteFramebuffers(1, &m_framebuffer);
}

void Renderer::Resize(int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (m_width == width && m_height == height)
    {
        return;
    }

    m_width = width;
    m_height = height;
    RebuildFramebuffer();
}

void Renderer::SetModel(const std::optional<ModelData>& model)
{
    ReleaseModel();
    m_model = model;
    if (!m_model)
    {
        return;
    }

    for (const MeshData& mesh : m_model->meshes)
    {
        GpuMesh gpuMesh;
        gpuMesh.indexCount = static_cast<int>(mesh.indices.size());
        gpuMesh.baseVertices = mesh.vertices;
        gpuMesh.dynamicVertices = mesh.vertices;

        glCreateVertexArrays(1, &gpuMesh.vao);
        glCreateBuffers(1, &gpuMesh.vbo);
        glCreateBuffers(1, &gpuMesh.ebo);

        glNamedBufferData(gpuMesh.vbo, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)), mesh.vertices.data(), GL_DYNAMIC_DRAW);
        glNamedBufferData(gpuMesh.ebo, static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(unsigned int)), mesh.indices.data(), GL_STATIC_DRAW);

        glVertexArrayVertexBuffer(gpuMesh.vao, 0, gpuMesh.vbo, 0, sizeof(Vertex));
        glVertexArrayElementBuffer(gpuMesh.vao, gpuMesh.ebo);

        glEnableVertexArrayAttrib(gpuMesh.vao, 0);
        glEnableVertexArrayAttrib(gpuMesh.vao, 1);
        glEnableVertexArrayAttrib(gpuMesh.vao, 2);
        glVertexArrayAttribFormat(gpuMesh.vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
        glVertexArrayAttribFormat(gpuMesh.vao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
        glVertexArrayAttribFormat(gpuMesh.vao, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texCoord));
        glVertexArrayAttribBinding(gpuMesh.vao, 0, 0);
        glVertexArrayAttribBinding(gpuMesh.vao, 1, 0);
        glVertexArrayAttribBinding(gpuMesh.vao, 2, 0);
        m_meshes.push_back(gpuMesh);
    }
}

void Renderer::SetVfxGraph(const std::optional<VfxGraph>& graph)
{
    m_vfxGraph = graph;
    m_particleTexture.reset();
    if (!m_vfxGraph)
    {
        return;
    }

    for (const VfxEmitter& emitter : m_vfxGraph->emitters)
    {
        if (!emitter.texturePath.empty())
        {
            m_particleTexture = TextureLoader::Load(emitter.texturePath);
            if (m_particleTexture)
            {
                return;
            }
        }
    }

    for (const VfxMaterialRef& material : m_vfxGraph->materials)
    {
        if (!material.texturePath.empty())
        {
            m_particleTexture = TextureLoader::Load(material.texturePath);
            if (m_particleTexture)
            {
                return;
            }
        }
    }
}

void Renderer::SetVfxParticles(const std::vector<ParticleRenderData>& particles)
{
    m_vfxParticles = particles;
}

void Renderer::SetSkinningMatrices(const ModelData* model, const std::vector<glm::mat4>& globalPose)
{
    if (!m_model || m_meshes.empty())
    {
        return;
    }

    if (model == nullptr || globalPose.empty() || !m_skinningEnabled)
    {
        for (std::size_t meshIndex = 0; meshIndex < m_meshes.size(); ++meshIndex)
        {
            const auto& vertices = m_meshes[meshIndex].baseVertices;
            m_meshes[meshIndex].dynamicVertices = vertices;
            glNamedBufferSubData(
                m_meshes[meshIndex].vbo,
                0,
                static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                vertices.data()
            );
        }
        return;
    }

    for (std::size_t meshIndex = 0; meshIndex < m_meshes.size(); ++meshIndex)
    {
        const Skeleton& skeleton = model->skeleton;
        const MeshData& meshData = model->meshes[meshIndex];
        std::vector<glm::mat4> skinningMatrices(skeleton.bones.size(), glm::mat4(1.0f));
        const std::size_t boneCount = std::min<std::size_t>(globalPose.size(), skeleton.bones.size());
        for (std::size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
        {
            const glm::mat4 currentPoseInModelSpace = model->globalInverseTransform * globalPose[boneIndex];
            if (boneIndex < meshData.boneOffsetMatrices.size())
            {
                skinningMatrices[boneIndex] = currentPoseInModelSpace * meshData.boneOffsetMatrices[boneIndex];
            }
        }

        m_meshes[meshIndex].dynamicVertices = m_meshes[meshIndex].baseVertices;
        for (Vertex& vertex : m_meshes[meshIndex].dynamicVertices)
        {
            const float totalWeight =
                vertex.boneWeights.x +
                vertex.boneWeights.y +
                vertex.boneWeights.z +
                vertex.boneWeights.w;

            if (totalWeight <= 0.0f)
            {
                continue;
            }

            glm::mat4 skinMatrix(0.0f);
            for (int slot = 0; slot < 4; ++slot)
            {
                const int boneIndex = vertex.boneIndices[slot];
                const float weight = vertex.boneWeights[slot];
                if (weight > 0.0f && boneIndex >= 0 && boneIndex < static_cast<int>(skinningMatrices.size()))
                {
                    skinMatrix += skinningMatrices[boneIndex] * weight;
                }
            }

            vertex.position = glm::vec3(skinMatrix * glm::vec4(vertex.position, 1.0f));
            vertex.normal = glm::normalize(glm::mat3(skinMatrix) * vertex.normal);
        }

        glNamedBufferSubData(
            m_meshes[meshIndex].vbo,
            0,
            static_cast<GLsizeiptr>(m_meshes[meshIndex].dynamicVertices.size() * sizeof(Vertex)),
            m_meshes[meshIndex].dynamicVertices.data()
        );
    }
}

void Renderer::Render(const OrbitCamera& camera, const ViewportOptions& options)
{
    RebuildVfxVertices(camera);

    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glViewport(0, 0, m_width, m_height);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.09f, 0.09f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (options.backfaceCulling) glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);

    glPolygonMode(GL_FRONT_AND_BACK, options.wireframe ? GL_LINE : GL_FILL);

    const glm::mat4 viewProjection = camera.ProjectionMatrix() * camera.ViewMatrix();

    if (options.showGrid)
    {
        m_gridShader.Bind();
        m_gridShader.SetMat4("uViewProjection", viewProjection);
        m_gridShader.SetVec3("uColor", glm::vec3(0.25f, 0.27f, 0.30f));
        glBindVertexArray(m_gridVao);
        glDrawArrays(GL_LINES, 0, m_gridVertexCount);
    }

    if (m_model)
    {
        m_staticModelShader.Bind();
        m_staticModelShader.SetMat4("uViewProjection", viewProjection);
        m_staticModelShader.SetMat4("uModel", glm::mat4(1.0f));
        m_staticModelShader.SetVec3("uCameraPosition", camera.Position());
        for (const GpuMesh& mesh : m_meshes)
        {
            glBindVertexArray(mesh.vao);
            glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
        }
    }

    if (options.showSkeleton && m_skeletonVertexCount > 0)
    {
        glDisable(GL_CULL_FACE);
        m_gridShader.Bind();
        m_gridShader.SetMat4("uViewProjection", viewProjection);
        m_gridShader.SetVec3("uColor", glm::vec3(0.95f, 0.55f, 0.15f));
        glBindVertexArray(m_skeletonVao);
        glDrawArrays(GL_LINES, 0, m_skeletonVertexCount);
    }

    if (options.showVfx && m_vfxEnabled && !m_vfxVertices.empty())
    {
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);

        m_particleShader.Bind();
        m_particleShader.SetMat4("uViewProjection", viewProjection);
        m_particleShader.SetInt("uParticleTexture", 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_particleTexture ? m_particleTexture->Id() : m_defaultParticleTexture);
        glBindVertexArray(m_particleVao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vfxVertices.size()));

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::SetSkeletonDebugLines(const std::vector<glm::vec3>& lineVertices)
{
    m_skeletonVertexCount = static_cast<int>(lineVertices.size());
    glNamedBufferData(
        m_skeletonVbo,
        static_cast<GLsizeiptr>(lineVertices.size() * sizeof(glm::vec3)),
        lineVertices.empty() ? nullptr : lineVertices.data(),
        GL_DYNAMIC_DRAW
    );
}

void Renderer::RebuildFramebuffer()
{
    if (m_framebuffer == 0) glCreateFramebuffers(1, &m_framebuffer);
    if (m_colorTexture != 0) glDeleteTextures(1, &m_colorTexture);
    if (m_depthTexture != 0) glDeleteTextures(1, &m_depthTexture);

    glCreateTextures(GL_TEXTURE_2D, 1, &m_colorTexture);
    glTextureStorage2D(m_colorTexture, 1, GL_RGBA8, m_width, m_height);
    glTextureParameteri(m_colorTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_colorTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glCreateTextures(GL_TEXTURE_2D, 1, &m_depthTexture);
    glTextureStorage2D(m_depthTexture, 1, GL_DEPTH24_STENCIL8, m_width, m_height);

    glNamedFramebufferTexture(m_framebuffer, GL_COLOR_ATTACHMENT0, m_colorTexture, 0);
    glNamedFramebufferTexture(m_framebuffer, GL_DEPTH_STENCIL_ATTACHMENT, m_depthTexture, 0);

    if (glCheckNamedFramebufferStatus(m_framebuffer, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        throw std::runtime_error("Viewport framebuffer is incomplete");
    }
}

void Renderer::ReleaseModel()
{
    for (const GpuMesh& mesh : m_meshes)
    {
        if (mesh.ebo != 0) glDeleteBuffers(1, &mesh.ebo);
        if (mesh.vbo != 0) glDeleteBuffers(1, &mesh.vbo);
        if (mesh.vao != 0) glDeleteVertexArrays(1, &mesh.vao);
    }
    m_meshes.clear();
}

void Renderer::RebuildVfxVertices(const OrbitCamera& camera)
{
    m_vfxVertices.clear();
    if (m_vfxParticles.empty())
    {
        glNamedBufferData(m_particleVbo, 0, nullptr, GL_DYNAMIC_DRAW);
        return;
    }

    const glm::mat4 inverseView = glm::inverse(camera.ViewMatrix());
    const glm::vec3 cameraRight = glm::normalize(glm::vec3(inverseView[0]));
    const glm::vec3 cameraUp = glm::normalize(glm::vec3(inverseView[1]));

    static constexpr std::array<glm::vec2, 6> kCorners = {
        glm::vec2(-1.0f, -1.0f),
        glm::vec2(1.0f, -1.0f),
        glm::vec2(1.0f, 1.0f),
        glm::vec2(-1.0f, -1.0f),
        glm::vec2(1.0f, 1.0f),
        glm::vec2(-1.0f, 1.0f)
    };

    static constexpr std::array<glm::vec2, 6> kUvs = {
        glm::vec2(0.0f, 0.0f),
        glm::vec2(1.0f, 0.0f),
        glm::vec2(1.0f, 1.0f),
        glm::vec2(0.0f, 0.0f),
        glm::vec2(1.0f, 1.0f),
        glm::vec2(0.0f, 1.0f)
    };

    m_vfxVertices.reserve(m_vfxParticles.size() * kCorners.size());
    for (const ParticleRenderData& particle : m_vfxParticles)
    {
        const glm::vec3 rightOffset = cameraRight * particle.size * 0.5f;
        const glm::vec3 upOffset = cameraUp * particle.size * 0.5f;
        for (std::size_t vertexIndex = 0; vertexIndex < kCorners.size(); ++vertexIndex)
        {
            const glm::vec2 corner = kCorners[vertexIndex];
            VfxVertex vertex;
            vertex.position = particle.position + rightOffset * corner.x + upOffset * corner.y;
            vertex.uv = kUvs[vertexIndex];
            vertex.color = particle.color;
            m_vfxVertices.push_back(vertex);
        }
    }

    glNamedBufferData(
        m_particleVbo,
        static_cast<GLsizeiptr>(m_vfxVertices.size() * sizeof(VfxVertex)),
        m_vfxVertices.data(),
        GL_DYNAMIC_DRAW
    );
}

void Renderer::CreateFallbackParticleTexture()
{
    const std::array<unsigned char, 4> whitePixel = {255, 255, 255, 255};
    glCreateTextures(GL_TEXTURE_2D, 1, &m_defaultParticleTexture);
    glTextureStorage2D(m_defaultParticleTexture, 1, GL_RGBA8, 1, 1);
    glTextureSubImage2D(m_defaultParticleTexture, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel.data());
    glTextureParameteri(m_defaultParticleTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_defaultParticleTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(m_defaultParticleTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_defaultParticleTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
}
