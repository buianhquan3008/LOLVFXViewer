#pragma once

#include "vfx/Particle.h"
#include "vfx/VfxGraph.h"

#include <optional>
#include <random>
#include <string>
#include <vector>

namespace lol
{
class VfxRuntime
{
public:
    void SetGraph(const std::optional<VfxGraph>& graph);
    void Clear();
    void Restart();
    void Update(float deltaSeconds, bool paused);

    const std::optional<VfxGraph>& Graph() const { return m_graph; }
    const std::vector<ParticleRenderData>& RenderParticles() const { return m_renderParticles; }
    std::size_t ParticleCount() const { return m_particles.size(); }
    float ElapsedSeconds() const { return m_elapsedSeconds; }
    bool Empty() const { return !m_graph.has_value(); }

private:
    struct EmitterState
    {
        float spawnAccumulator = 0.0f;
        bool burstFired = false;
    };

    void RebuildEmitterStates();
    void SpawnFromEmitter(const VfxEmitter& emitter, EmitterState& state, std::size_t emitterIndex, int count);
    void RebuildRenderData();

    std::optional<VfxGraph> m_graph;
    std::vector<EmitterState> m_emitterStates;
    std::vector<Particle> m_particles;
    std::vector<ParticleRenderData> m_renderParticles;
    std::mt19937 m_rng{1337u};
    float m_elapsedSeconds = 0.0f;
};
}
