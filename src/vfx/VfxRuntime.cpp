#include "vfx/VfxRuntime.h"

#include <algorithm>
#include <cmath>

namespace lol
{
namespace
{
float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}
}

void VfxRuntime::SetGraph(const std::optional<VfxGraph>& graph)
{
    m_graph = graph;
    Restart();
}

void VfxRuntime::Clear()
{
    m_graph.reset();
    m_emitterStates.clear();
    m_particles.clear();
    m_renderParticles.clear();
    m_elapsedSeconds = 0.0f;
}

void VfxRuntime::Restart()
{
    m_particles.clear();
    m_renderParticles.clear();
    m_elapsedSeconds = 0.0f;
    RebuildEmitterStates();
}

void VfxRuntime::Update(float deltaSeconds, bool paused)
{
    if (!m_graph)
    {
        return;
    }

    if (paused)
    {
        RebuildRenderData();
        return;
    }

    m_elapsedSeconds += deltaSeconds;
    if (m_graph->durationSeconds > 0.0f && m_elapsedSeconds > m_graph->durationSeconds)
    {
        if (m_graph->loop)
        {
            m_elapsedSeconds = std::fmod(m_elapsedSeconds, m_graph->durationSeconds);
            for (EmitterState& state : m_emitterStates)
            {
                state.burstFired = false;
            }
        }
    }

    const bool effectActive = m_graph->loop || m_elapsedSeconds <= m_graph->durationSeconds;
    for (std::size_t emitterIndex = 0; emitterIndex < m_graph->emitters.size(); ++emitterIndex)
    {
        const VfxEmitter& emitter = m_graph->emitters[emitterIndex];
        EmitterState& state = m_emitterStates[emitterIndex];
        if (!effectActive && !emitter.loop)
        {
            continue;
        }

        if (!state.burstFired && emitter.burstCount > 0.0f)
        {
            SpawnFromEmitter(emitter, state, emitterIndex, std::max(1, static_cast<int>(emitter.burstCount)));
            state.burstFired = true;
        }

        if (emitter.spawnRate > 0.0f)
        {
            state.spawnAccumulator += emitter.spawnRate * deltaSeconds;
            const int spawnCount = static_cast<int>(state.spawnAccumulator);
            if (spawnCount > 0)
            {
                state.spawnAccumulator -= static_cast<float>(spawnCount);
                SpawnFromEmitter(emitter, state, emitterIndex, spawnCount);
            }
        }
    }

    for (Particle& particle : m_particles)
    {
        particle.ageSeconds += deltaSeconds;
        particle.position += particle.velocity * deltaSeconds;
    }

    std::erase_if(m_particles, [](const Particle& particle) {
        return particle.ageSeconds >= particle.lifetimeSeconds;
    });

    RebuildRenderData();
}

void VfxRuntime::RebuildEmitterStates()
{
    m_emitterStates.clear();
    if (m_graph)
    {
        m_emitterStates.resize(m_graph->emitters.size());
    }
}

void VfxRuntime::SpawnFromEmitter(const VfxEmitter& emitter, EmitterState& state, std::size_t emitterIndex, int count)
{
    (void)state;
    (void)emitterIndex;

    if (count <= 0)
    {
        return;
    }

    std::uniform_real_distribution<float> lifetimeDistribution(
        std::min(emitter.lifetimeMin, emitter.lifetimeMax),
        std::max(emitter.lifetimeMin, emitter.lifetimeMax)
    );
    std::uniform_real_distribution<float> velocityX(emitter.velocityMin.x, emitter.velocityMax.x);
    std::uniform_real_distribution<float> velocityY(emitter.velocityMin.y, emitter.velocityMax.y);
    std::uniform_real_distribution<float> velocityZ(emitter.velocityMin.z, emitter.velocityMax.z);

    for (int index = 0; index < count; ++index)
    {
        if (static_cast<int>(m_particles.size()) >= emitter.maxParticles)
        {
            break;
        }

        Particle particle;
        particle.position = emitter.localOffset;
        particle.velocity = glm::vec3(velocityX(m_rng), velocityY(m_rng), velocityZ(m_rng));
        particle.startColor = emitter.startColor;
        particle.endColor = emitter.endColor;
        particle.startSize = emitter.startSize;
        particle.endSize = emitter.endSize;
        particle.lifetimeSeconds = std::max(0.05f, lifetimeDistribution(m_rng));
        m_particles.push_back(particle);
    }
}

void VfxRuntime::RebuildRenderData()
{
    m_renderParticles.clear();
    m_renderParticles.reserve(m_particles.size());
    for (const Particle& particle : m_particles)
    {
        const float normalizedAge = std::clamp(particle.ageSeconds / std::max(particle.lifetimeSeconds, 0.001f), 0.0f, 1.0f);
        ParticleRenderData renderParticle;
        renderParticle.position = particle.position;
        renderParticle.color = glm::mix(particle.startColor, particle.endColor, normalizedAge);
        renderParticle.size = Lerp(particle.startSize, particle.endSize, normalizedAge);
        m_renderParticles.push_back(renderParticle);
    }
}
}
