#pragma once

#include "assets/ModelLoader.h"
#include "animation/Skeleton.h"
#include "renderer/Camera.h"
#include "renderer/Shader.h"

#include <array>
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
};

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void Resize(int width, int height);
    void SetModel(const std::optional<ModelData>& model);
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

    void RebuildFramebuffer();
    void ReleaseModel();
    void DrawGrid(const OrbitCamera& camera);

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
    Shader m_staticModelShader;
    Shader m_gridShader;
    std::vector<GpuMesh> m_meshes;
    std::optional<ModelData> m_model;
};
}
