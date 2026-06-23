#pragma once

#include "assets/ModelLoader.h"
#include "animation/Skeleton.h"
#include "renderer/Camera.h"
#include "renderer/Shader.h"
#include "renderer/Texture2D.h"
#include "vfx/Particle.h"
#include "vfx/VfxGraph.h"

#include <array>
#include <filesystem>
#include <optional>
#include <vector>

namespace lol
{
struct ViewportOptions
{
    bool showGrid = true;
    bool wireframe = false;
    bool backfaceCulling = true;
    bool showBounds = true;
    bool showSkeleton = true;
    bool showVfx = true;
};

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void Resize(int width, int height);
    void SetModel(const std::optional<ModelData>& model);
    void SetVfxGraph(const std::optional<VfxGraph>& graph);
    void SetVfxEnabled(bool enabled) { m_vfxEnabled = enabled; }
    void SetVfxParticles(const std::vector<ParticleRenderData>& particles);
    void SetSkinningEnabled(bool enabled) { m_skinningEnabled = enabled; }
    void SetSkinningMatrices(const ModelData* model, const std::vector<glm::mat4>& globalPose);
    void SetSkeletonDebugLines(const std::vector<glm::vec3>& lineVertices);
    void Render(const OrbitCamera& camera, const ViewportOptions& options);

    unsigned int OutputTexture() const { return m_colorTexture; }

private:
    struct GpuMesh
    {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ebo = 0;
        int indexCount = 0;
        std::vector<Vertex> baseVertices;
        std::vector<Vertex> dynamicVertices;
    };

    struct VfxVertex
    {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec2 uv = glm::vec2(0.0f);
        glm::vec4 color = glm::vec4(1.0f);
    };

    void RebuildFramebuffer();
    void ReleaseModel();
    void RebuildVfxVertices(const OrbitCamera& camera);
    void DrawGrid(const OrbitCamera& camera);
    void CreateFallbackParticleTexture();

    int m_width = 1280;
    int m_height = 720;
    unsigned int m_framebuffer = 0;
    unsigned int m_colorTexture = 0;
    unsigned int m_depthTexture = 0;
    unsigned int m_gridVao = 0;
    unsigned int m_gridVbo = 0;
    int m_gridVertexCount = 0;
    unsigned int m_skeletonVao = 0;
    unsigned int m_skeletonVbo = 0;
    int m_skeletonVertexCount = 0;
    bool m_skinningEnabled = true;
    bool m_vfxEnabled = true;
    Shader m_staticModelShader;
    Shader m_gridShader;
    Shader m_particleShader;
    std::vector<GpuMesh> m_meshes;
    std::optional<ModelData> m_model;
    std::optional<VfxGraph> m_vfxGraph;
    std::vector<ParticleRenderData> m_vfxParticles;
    std::vector<VfxVertex> m_vfxVertices;
    unsigned int m_particleVao = 0;
    unsigned int m_particleVbo = 0;
    unsigned int m_defaultParticleTexture = 0;
    std::optional<Texture2D> m_particleTexture;
};
}
