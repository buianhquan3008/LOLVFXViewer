#include "core/Application.h"

#include "core/Log.h"

#include <glad/gl.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <chrono>
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace lol
{
Application::Application()
    : m_window(1600, 900, "LoL Asset Studio")
{
    Log::Push(LogLevel::Info, "Window created successfully");

    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0)
    {
        throw std::runtime_error("Failed to initialize GLAD");
    }
    Log::Push(LogLevel::Info, "GLAD initialized");

    ValidateOpenGL();

    m_renderer = std::make_unique<Renderer>();
    Log::Push(LogLevel::Info, "Renderer initialized");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(m_window.NativeHandle(), true);
    ImGui_ImplOpenGL3_Init("#version 460");
    Log::Push(LogLevel::Info, "ImGui initialized");

    std::filesystem::path startupRoot = std::filesystem::current_path();
    const std::filesystem::path dataFolder = startupRoot / "data";
    if (std::filesystem::exists(dataFolder) && std::filesystem::is_directory(dataFolder))
    {
        startupRoot = dataFolder;
    }

    m_state.rootPathInput = startupRoot.string();
    m_state.assetManager.SetRoot(startupRoot);
    m_state.currentDirectory = startupRoot;
    Log::Push(LogLevel::Info, "Application initialized");
}

Application::~Application()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Application::Run()
{
    auto last = std::chrono::steady_clock::now();

    while (!m_window.ShouldClose())
    {
        const auto now = std::chrono::steady_clock::now();
        m_timeStep.deltaSeconds = std::clamp(std::chrono::duration<float>(now - last).count(), 0.0f, 1.0f / 30.0f);
        last = now;

        m_window.PollEvents();
        m_state.animator.SetPlaying(!m_state.pauseAnimation);
        m_state.animator.Update(m_timeStep.deltaSeconds);
        m_state.vfxRuntime.Update(m_timeStep.deltaSeconds, m_state.pauseVfx || !m_state.enableVfx);
        m_state.camera.Update(m_timeStep.deltaSeconds, m_state.viewportHovered, m_state.viewportFocused);
        SyncRendererSelection();
        m_renderer->SetSkinningEnabled(m_state.enableSkinning);
        if (m_state.selection.model)
        {
            if (m_state.enableSkinning)
            {
                m_renderer->SetSkinningMatrices(&*m_state.selection.model, m_state.animator.GlobalPose());
            }
            else
            {
                m_renderer->SetSkinningMatrices(nullptr, {});
            }
        }
        else
        {
            m_renderer->SetSkinningMatrices(nullptr, {});
        }
        if (m_state.selection.model)
        {
            m_renderer->SetSkeletonDebugLines(m_state.animator.BuildDebugLineVertices(m_state.selection.model->globalInverseTransform));
        }
        else
        {
            m_renderer->SetSkeletonDebugLines({});
        }
        m_renderer->SetVfxEnabled(m_state.enableVfx);
        m_renderer->SetVfxParticles(m_state.vfxRuntime.RenderParticles());
        m_renderer->Render(m_state.camera, m_state.viewportOptions);

        BeginFrame();
        DrawDockspace();
        m_assetBrowserPanel.Draw(m_state);
        m_viewportPanel.Draw(m_state, *m_renderer);
        m_animationPanel.Draw(m_state);
        m_inspectorPanel.Draw(m_state);
        m_vfxPanel.Draw(m_state);
        m_consolePanel.Draw();
        EndFrame();
    }
}

void Application::BeginFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Application::DrawDockspace()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("DockSpaceRoot", nullptr, flags);
    ImGui::PopStyleVar(2);

    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit"))
            {
                glfwSetWindowShouldClose(m_window.NativeHandle(), GLFW_TRUE);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::End();
}

void Application::EndFrame()
{
    ImGui::Render();

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    m_window.GetFramebufferSize(framebufferWidth, framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    m_window.SwapBuffers();
}

void Application::SyncRendererSelection()
{
    if (m_state.selection.type == AssetSelectionType::Model && m_state.selection.model)
    {
        if (m_lastRenderedModelPath != m_state.selection.path)
        {
            m_renderer->SetModel(m_state.selection.model);
            m_renderer->SetVfxGraph(std::nullopt);
            m_state.animator.SetData(&m_state.selection.model->skeleton, &m_state.selection.model->animationClips);
            m_state.selectedBoneIndex = m_state.selection.model->skeleton.Empty() ? -1 : 0;
            m_state.pauseAnimation = false;
            m_state.camera.FocusBounds(m_state.selection.model->boundsMin, m_state.selection.model->boundsMax);
            m_lastRenderedModelPath = m_state.selection.path;
            m_lastRenderedVfxPath.clear();
            m_state.vfxRuntime.Clear();
            Log::Push(
                LogLevel::Info,
                "Loaded model animation data: bones=" + std::to_string(m_state.selection.model->skeleton.bones.size()) +
                    ", clips=" + std::to_string(m_state.selection.model->animationClips.size()) +
                    ", meshes=" + std::to_string(m_state.selection.model->meshes.size()),
                m_state.selection.path.string()
            );
        }
    }
    else
    {
        if (!m_lastRenderedModelPath.empty())
        {
            m_renderer->SetModel(std::nullopt);
            m_renderer->SetSkeletonDebugLines({});
            m_state.animator.Clear();
            m_state.selectedBoneIndex = -1;
            m_lastRenderedModelPath.clear();
        }
    }

    if (m_state.selection.type == AssetSelectionType::Vfx && m_state.selection.vfx)
    {
        if (m_lastRenderedVfxPath != m_state.selection.path)
        {
            m_renderer->SetModel(std::nullopt);
            m_renderer->SetSkeletonDebugLines({});
            m_state.animator.Clear();
            m_state.selectedBoneIndex = -1;
            m_state.vfxRuntime.SetGraph(m_state.selection.vfx);
            m_renderer->SetVfxGraph(m_state.selection.vfx);
            m_state.pauseVfx = false;
            m_state.enableVfx = true;
            m_state.camera.FocusBounds(glm::vec3(-1.0f), glm::vec3(1.0f));
            m_lastRenderedModelPath.clear();
            m_lastRenderedVfxPath = m_state.selection.path;
            Log::Push(
                LogLevel::Info,
                "Loaded VFX graph: emitters=" + std::to_string(m_state.selection.vfx->emitters.size()) +
                    ", materials=" + std::to_string(m_state.selection.vfx->materials.size()) +
                    ", children=" + std::to_string(m_state.selection.vfx->children.size()),
                m_state.selection.path.string()
            );
        }
        return;
    }

    if (!m_lastRenderedVfxPath.empty())
    {
        m_renderer->SetVfxGraph(std::nullopt);
        m_renderer->SetVfxParticles({});
        m_state.vfxRuntime.Clear();
        m_lastRenderedVfxPath.clear();
    }
}

void Application::ValidateOpenGL()
{
    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);

    if (vendor == nullptr || renderer == nullptr || version == nullptr)
    {
        throw std::runtime_error("OpenGL context is invalid: glGetString returned null.");
    }

    std::ostringstream stream;
    stream << "OpenGL vendor: " << reinterpret_cast<const char*>(vendor)
           << " | renderer: " << reinterpret_cast<const char*>(renderer)
           << " | version: " << reinterpret_cast<const char*>(version);
    Log::Push(LogLevel::Info, stream.str());

    int major = 0;
    int minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);

    if (major < 4 || (major == 4 && minor < 5))
    {
        std::ostringstream versionError;
        versionError << "OpenGL 4.5 or newer is required, but got " << major << "." << minor;
        throw std::runtime_error(versionError.str());
    }

    struct FunctionCheck
    {
        const char* name;
        void* pointer;
    };

    const FunctionCheck requiredFunctions[] = {
        {"glCreateTextures", reinterpret_cast<void*>(glCreateTextures)},
        {"glCreateBuffers", reinterpret_cast<void*>(glCreateBuffers)},
        {"glCreateVertexArrays", reinterpret_cast<void*>(glCreateVertexArrays)},
        {"glNamedBufferData", reinterpret_cast<void*>(glNamedBufferData)},
        {"glTextureStorage2D", reinterpret_cast<void*>(glTextureStorage2D)},
        {"glNamedFramebufferTexture", reinterpret_cast<void*>(glNamedFramebufferTexture)},
        {"glCheckNamedFramebufferStatus", reinterpret_cast<void*>(glCheckNamedFramebufferStatus)},
    };

    for (const auto& function : requiredFunctions)
    {
        if (function.pointer == nullptr)
        {
            throw std::runtime_error(std::string("Required OpenGL function pointer is null: ") + function.name);
        }
    }
}
}
