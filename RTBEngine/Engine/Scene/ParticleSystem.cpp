#include "ParticleSystem.h"

#include "GameObject.h"
#include "ObjectPool.h"
#include "Scene.h"
#include "SceneManager.h"
#include "../Core/ResourceManager.h"
#include "../Rendering/Camera.h"
#include "../Rendering/CameraUBO.h"
#include "../Rendering/FogUniforms.h"
#include "../Rendering/Shader.h"
#include "../Rendering/Texture.h"
#include "../Rendering/RHI/RenderDevice.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>

namespace RTBEngine {
    namespace Scene {

        namespace {
            constexpr float kMinParticleSize = 0.001f;
            constexpr float kMinLifetime = 0.05f;
            constexpr float kMinEmissionRate = 0.0f;
            constexpr int kMinMaxParticles = 1;
            constexpr int kMaxMaxParticles = 8192;

            RTBEngine::Rendering::RHI::IRenderDevice& Device()
            {
                return RTBEngine::Rendering::RHI::RenderDevice::Get();
            }

            struct QuadVertex {
                Math::Vector2 corner;
                Math::Vector2 uv;
            };

            struct ParticleSharedResources {
                Rendering::RHI::GpuId quadVao = Rendering::RHI::kInvalidGpuId;
                Rendering::RHI::GpuId quadVbo = Rendering::RHI::kInvalidGpuId;
                int refCount = 0;
            };

            ParticleSharedResources g_particleShared;
            std::mt19937& GetRandomEngine()
            {
                static std::mt19937 engine{ std::random_device{}() };
                return engine;
            }

            float RandomRange(float minValue, float maxValue)
            {
                std::uniform_real_distribution<float> dist(minValue, maxValue);
                return dist(GetRandomEngine());
            }

            Math::Vector3 RandomUnitVector()
            {
                const float z = RandomRange(-1.0f, 1.0f);
                const float theta = RandomRange(0.0f, 6.28318530718f);
                const float radius = std::sqrt(std::max(0.0f, 1.0f - z * z));
                return Math::Vector3(radius * std::cos(theta), z, radius * std::sin(theta));
            }

            // Creates the engine-wide unit quad VAO/VBO used by every ParticleSystem instance.
            // Attributes 0 (corner) and 1 (uv) are per-vertex; instance data is bound separately.
            bool EnsureSharedQuadResources()
            {
                if (g_particleShared.quadVao != Rendering::RHI::kInvalidGpuId
                    && g_particleShared.quadVbo != Rendering::RHI::kInvalidGpuId) {
                    return true;
                }

                const QuadVertex vertices[] = {
                    { Math::Vector2(-0.5f, -0.5f), Math::Vector2(0.0f, 0.0f) },
                    { Math::Vector2(0.5f, -0.5f), Math::Vector2(1.0f, 0.0f) },
                    { Math::Vector2(0.5f, 0.5f), Math::Vector2(1.0f, 1.0f) },
                    { Math::Vector2(-0.5f, -0.5f), Math::Vector2(0.0f, 0.0f) },
                    { Math::Vector2(0.5f, 0.5f), Math::Vector2(1.0f, 1.0f) },
                    { Math::Vector2(-0.5f, 0.5f), Math::Vector2(0.0f, 1.0f) },
                };

                auto& device = Device();
                g_particleShared.quadVao = device.CreateVertexArray();
                g_particleShared.quadVbo = device.CreateBuffer();

                device.BindVertexArray(g_particleShared.quadVao);
                device.SetArrayBufferData(
                    g_particleShared.quadVbo,
                    vertices,
                    sizeof(vertices),
                    Rendering::RHI::BufferUsage::Static);
                device.EnableVertexAttribFloat(
                    0, 2, static_cast<int>(sizeof(QuadVertex)), offsetof(QuadVertex, corner));
                device.EnableVertexAttribFloat(
                    1, 2, static_cast<int>(sizeof(QuadVertex)), offsetof(QuadVertex, uv));
                device.UnbindVertexArray();
                return g_particleShared.quadVao != Rendering::RHI::kInvalidGpuId;
            }

            void ReleaseSharedQuadResources()
            {
                if (g_particleShared.refCount > 0) {
                    return;
                }

                if (!Rendering::RHI::RenderDevice::HasDevice()) {
                    g_particleShared.quadVao = Rendering::RHI::kInvalidGpuId;
                    g_particleShared.quadVbo = Rendering::RHI::kInvalidGpuId;
                    return;
                }

                auto& device = Device();
                if (g_particleShared.quadVao != Rendering::RHI::kInvalidGpuId) {
                    device.DestroyVertexArray(g_particleShared.quadVao);
                    g_particleShared.quadVao = Rendering::RHI::kInvalidGpuId;
                }
                if (g_particleShared.quadVbo != Rendering::RHI::kInvalidGpuId) {
                    device.DestroyBuffer(g_particleShared.quadVbo);
                    g_particleShared.quadVbo = Rendering::RHI::kInvalidGpuId;
                }
            }
        }

        using ThisClass = ParticleSystem;
        RTB_REGISTER_COMPONENT(ParticleSystem)
            RTB_PROPERTY_RANGE(maxParticles, 1, 8192)
            RTB_PROPERTY_RANGE(emissionRate, 0.0f, 1000.0f)
            RTB_PROPERTY_ENUM(emitterShape, "Point", "Sphere", "Cone", "Box", "Line", "Orbit")
            RTB_PROPERTY_RANGE(shapeRadius, 0.0f, 10.0f)
            RTB_PROPERTY_RANGE(coneAngle, 0.0f, 90.0f)
            RTB_PROPERTY_RANGE(lineLength, 0.0f, 100.0f)
            RTB_PROPERTY_RANGE(orbitRadius, 0.0f, 100.0f)
            RTB_PROPERTY_RANGE(orbitSpeed, 0.0f, 50.0f)
            RTB_PROPERTY(boxSize)
            RTB_PROPERTY_RANGE(startLifetime, 0.05f, 30.0f)
            RTB_PROPERTY_RANGE(startSpeed, 0.0f, 100.0f)
            RTB_PROPERTY_RANGE(startSize, 0.001f, 10.0f)
            RTB_PROPERTY_RANGE(endSize, 0.001f, 10.0f)
            RTB_PROPERTY_COLOR(startColor)
            RTB_PROPERTY_COLOR(endColor)
            RTB_PROPERTY(gravity)
            RTB_PROPERTY(worldSimulation)
            RTB_PROPERTY_TEXTURE(textureRef)
            RTB_PROPERTY(visible)
            RTB_PROPERTY(loop)
            RTB_PROPERTY(playOnAwake)
            RTB_PROPERTY(simulateInEditMode)
            RTB_PROPERTY(destroyOwnerWhenFinished)
            RTB_PROPERTY_RANGE(burstCount, 1, 256)
            RTB_PROPERTY_RANGE(textureSheetColumns, 1, 32)
            RTB_PROPERTY_RANGE(textureSheetRows, 1, 32)
            RTB_PROPERTY_RANGE(textureSheetFrameCount, 1, 1024)
            RTB_PROPERTY_RANGE(textureSheetFramesPerSecond, 0.0f, 120.0f)
            RTB_PROPERTY_ENUM(blendMode, "Alpha", "Additive")
        RTB_END_REGISTER(ParticleSystem)

        ParticleSystem::ParticleSystem()
            : Component()
        {
        }

        ParticleSystem::~ParticleSystem()
        {
            ReleaseRenderResources();
        }

        // Allocates the CPU particle pool to match maxParticles.
        void ParticleSystem::OnAwake()
        {
            ResizePool();
        }

        // Starts playback when playOnAwake is set and the user has not called Stop().
        void ParticleSystem::OnStart()
        {
            ApplyPlaybackSettings();
        }

        void ParticleSystem::OnPoolAcquire()
        {
            Restart();
        }

        void ParticleSystem::OnPoolRelease()
        {
            Stop();
        }

        // Advances simulation, refreshes the GPU instance buffer, and destroys the owner when configured.
        void ParticleSystem::OnUpdate(float deltaTime)
        {
            Tick(deltaTime);
            TryDestroyOwnerWhenFinished();
        }

        // Clamps reflected ranges, resizes the pool, and reapplies playback settings.
        void ParticleSystem::OnValidate()
        {
            maxParticles = std::clamp(maxParticles, kMinMaxParticles, kMaxMaxParticles);
            emissionRate = std::max(emissionRate, kMinEmissionRate);
            startLifetime = std::max(startLifetime, kMinLifetime);
            startSize = std::max(startSize, kMinParticleSize);
            endSize = std::max(endSize, kMinParticleSize);
            shapeRadius = std::max(shapeRadius, 0.0f);
            coneAngle = std::clamp(coneAngle, 0.0f, 90.0f);
            boxSize.x = std::max(boxSize.x, 0.0f);
            boxSize.y = std::max(boxSize.y, 0.0f);
            boxSize.z = std::max(boxSize.z, 0.0f);
            burstCount = std::clamp(burstCount, 1, 256);
            textureSheetColumns = std::clamp(textureSheetColumns, 1, 32);
            textureSheetRows = std::clamp(textureSheetRows, 1, 32);
            textureSheetFrameCount = std::clamp(textureSheetFrameCount, 1, 1024);
            textureSheetFramesPerSecond = std::max(textureSheetFramesPerSecond, 0.0f);
            if (loop) {
                destroyOwnerWhenFinished = false;
            }
            ResizePool();
            ApplyPlaybackSettings();
        }

        void ParticleSystem::OnDestroy()
        {
            ReleaseRenderResources();
        }

        // Marks every slot as dead (lifetime == 0) and clears the active instance count.
        void ParticleSystem::KillAllParticles()
        {
            for (Rendering::Particle& particle : particles) {
                particle.age = 0.0f;
                particle.lifetime = 0.0f;
            }
            RebuildFreeSlots();
            activeInstanceCount = 0;
        }

        void ParticleSystem::ClearSimulation()
        {
            emissionAccumulator = 0.0f;
            totalEmitted = 0;
            hasCompletedPlayback = false;
            destroyOwnerTriggered = false;
            KillAllParticles();
            UploadInstanceBuffer();
        }

        bool ParticleSystem::IsBurstEmitter() const
        {
            return emissionRate <= kMinEmissionRate;
        }

        void ParticleSystem::BeginPlayback(bool emitBurstIfRateZero)
        {
            userStopped = false;
            playing = true;
            paused = false;
            hasCompletedPlayback = false;
            destroyOwnerTriggered = false;

            if (emitBurstIfRateZero && IsBurstEmitter()) {
                Emit(burstCount);
                UploadInstanceBuffer();
            }
        }

        void ParticleSystem::ApplyPlaybackSettings()
        {
            if (!playOnAwake || userStopped) {
                return;
            }

            Restart();
        }

        void ParticleSystem::RebuildFreeSlots()
        {
            freeSlots.clear();
            freeSlots.reserve(particles.size());
            activeSlots.clear();
            activeSlots.reserve(particles.size());
            activeParticleCount = 0;

            for (int i = 0; i < static_cast<int>(particles.size()); ++i) {
                if (particles[static_cast<std::size_t>(i)].IsAlive()) {
                    activeSlots.push_back(i);
                    ++activeParticleCount;
                } else {
                    freeSlots.push_back(i);
                }
            }
        }

        // Grows or shrinks the CPU pool and parallel instanceData array; new slots start dead.
        void ParticleSystem::ResizePool()
        {
            const int clampedMax = std::clamp(maxParticles, kMinMaxParticles, kMaxMaxParticles);
            maxParticles = clampedMax;

            const std::size_t previousSize = particles.size();
            particles.resize(static_cast<std::size_t>(clampedMax));
            instanceData.resize(static_cast<std::size_t>(clampedMax));

            if (particles.size() > previousSize) {
                freeSlots.reserve(particles.size());
                for (std::size_t i = previousSize; i < particles.size(); ++i) {
                    particles[i].age = 0.0f;
                    particles[i].lifetime = 0.0f;
                    freeSlots.push_back(static_cast<int>(i));
                }
            } else if (particles.size() < previousSize) {
                RebuildFreeSlots();
            } else if (previousSize == 0) {
                RebuildFreeSlots();
            }
        }

        // Resumes emission without clearing particles already alive in the pool.
        void ParticleSystem::Play()
        {
            BeginPlayback(false);
        }

        // Clears the pool, resets counters, and blocks playOnAwake until the next validate/load.
        void ParticleSystem::Stop()
        {
            userStopped = true;
            playing = false;
            paused = false;
            ClearSimulation();
        }

        void ParticleSystem::Restart()
        {
            ClearSimulation();
            BeginPlayback(true);
        }

        // Freezes simulation while keeping visible particles in the pool.
        void ParticleSystem::Pause()
        {
            if (playing) {
                paused = true;
            }
        }

        // Spawns count particles immediately (burst); also forces playback on.
        void ParticleSystem::Emit(int count)
        {
            if (count <= 0) {
                return;
            }

            playing = true;
            paused = false;
            hasCompletedPlayback = false;

            for (int i = 0; i < count; ++i) {
                SpawnParticle();
            }
        }

        int ParticleSystem::GetActiveParticleCount() const
        {
            return activeParticleCount;
        }

        // Editor-only: ticks emitters with simulateInEditMode while not in Play mode.
        void ParticleSystem::TickScenePreview(Scene* scene, float deltaTime)
        {
            if (!scene || deltaTime <= 0.0f) {
                return;
            }

            for (const auto& gameObjectPtr : scene->GetGameObjects()) {
                GameObject* gameObject = gameObjectPtr.get();
                if (!gameObject || !gameObject->IsActiveInHierarchy()) {
                    continue;
                }

                ParticleSystem* particleSystem = gameObject->GetComponent<ParticleSystem>();
                if (!particleSystem || !particleSystem->IsEnabled()) {
                    continue;
                }

                if (!particleSystem->simulateInEditMode || !particleSystem->IsPlaying()) {
                    continue;
                }

                particleSystem->Tick(deltaTime);
            }
        }

        // Uniform random direction inside a cone around local +Y; angle 0 emits along +Y only.
        Math::Vector3 ParticleSystem::SampleConeDirection() const
        {
            const float halfAngleRad = std::max(coneAngle, 0.0f) * 0.5f * 3.1415926535f / 180.0f;
            const float cosHalfAngle = std::cos(halfAngleRad);

            if (cosHalfAngle >= 1.0f - 1e-6f) {
                return Math::Vector3(0.0f, 1.0f, 0.0f);
            }

            const float cosTheta = RandomRange(cosHalfAngle, 1.0f);
            const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
            const float phi = RandomRange(0.0f, 6.28318530718f);
            return Math::Vector3(sinTheta * std::cos(phi), cosTheta, sinTheta * std::sin(phi));
        }

        // Local-space spawn offset based on the selected emitter shape.
        Math::Vector3 ParticleSystem::SampleSpawnPosition() const
        {
            switch (emitterShape) {
            case Rendering::ParticleEmitterShape::Point:
                return Math::Vector3::Zero();

            case Rendering::ParticleEmitterShape::Sphere: {
                const Math::Vector3 direction = RandomUnitVector();
                const float radius = RandomRange(0.0f, shapeRadius);
                return direction * radius;
            }

            case Rendering::ParticleEmitterShape::Cone:
                return SampleConeDirection() * RandomRange(0.0f, shapeRadius);

            case Rendering::ParticleEmitterShape::Box:
                return Math::Vector3(
                    RandomRange(-boxSize.x * 0.5f, boxSize.x * 0.5f),
                    RandomRange(-boxSize.y * 0.5f, boxSize.y * 0.5f),
                    RandomRange(-boxSize.z * 0.5f, boxSize.z * 0.5f));

            case Rendering::ParticleEmitterShape::Line: {
                const float halfLength = std::max(lineLength, 0.0f) * 0.5f;
                return Math::Vector3(RandomRange(-halfLength, halfLength), 0.0f, 0.0f);
            }

            case Rendering::ParticleEmitterShape::Orbit: {
                lastOrbitSpawnAngle = RandomRange(0.0f, 6.28318530718f);
                return Math::Vector3(
                    std::cos(lastOrbitSpawnAngle) * orbitRadius,
                    0.0f,
                    std::sin(lastOrbitSpawnAngle) * orbitRadius);
            }

            default:
                return Math::Vector3::Zero();
            }
        }

        // Local-space initial velocity direction before startSpeed is applied.
        Math::Vector3 ParticleSystem::SampleSpawnDirection() const
        {
            switch (emitterShape) {
            case Rendering::ParticleEmitterShape::Cone:
                return SampleConeDirection();

            case Rendering::ParticleEmitterShape::Sphere: {
                Math::Vector3 direction = RandomUnitVector();
                direction.Normalize();
                return direction;
            }

            case Rendering::ParticleEmitterShape::Line:
                return Math::Vector3(1.0f, 0.0f, 0.0f);

            case Rendering::ParticleEmitterShape::Orbit: {
                Math::Vector3 tangent(
                    -std::sin(lastOrbitSpawnAngle),
                    0.0f,
                    std::cos(lastOrbitSpawnAngle));
                tangent.Normalize();
                return tangent;
            }

            default:
                return RandomUnitVector().Normalized();
            }
        }

        // Converts a local particle position to world space when worldSimulation is false.
        Math::Vector3 ParticleSystem::ToWorldPosition(const Math::Vector3& localOrWorld) const
        {
            if (worldSimulation || !GetOwner()) {
                return localOrWorld;
            }

            const Math::Matrix4 worldMatrix = GetOwner()->GetWorldMatrix();
            const Math::Vector4 worldPos = worldMatrix * Math::Vector4(
                localOrWorld.x, localOrWorld.y, localOrWorld.z, 1.0f);
            return Math::Vector3(worldPos.x, worldPos.y, worldPos.z);
        }

        // Converts a local direction to world space when worldSimulation is false.
        Math::Vector3 ParticleSystem::ToWorldDirection(const Math::Vector3& localOrWorld) const
        {
            if (worldSimulation || !GetOwner()) {
                return localOrWorld;
            }

            const Math::Matrix4 worldMatrix = GetOwner()->GetWorldMatrix();
            const Math::Vector4 worldDir = worldMatrix * Math::Vector4(
                localOrWorld.x, localOrWorld.y, localOrWorld.z, 0.0f);
            Math::Vector3 result(worldDir.x, worldDir.y, worldDir.z);
            result.Normalize();
            return result;
        }

        // Claims a dead pool slot and initializes position, velocity, lifetime, size, and color.
        void ParticleSystem::SpawnParticle()
        {
            if (freeSlots.empty()) {
                return;
            }

            const int slot = freeSlots.back();
            freeSlots.pop_back();

            Rendering::Particle& particle = particles[static_cast<std::size_t>(slot)];
            const Math::Vector3 localPosition = SampleSpawnPosition();
            const float spawnSpeed = (emitterShape == Rendering::ParticleEmitterShape::Orbit)
                ? orbitSpeed
                : startSpeed;
            const Math::Vector3 localVelocity = SampleSpawnDirection() * spawnSpeed;

            if (worldSimulation) {
                if (GetOwner()) {
                    const Math::Matrix4 worldMatrix = GetOwner()->GetWorldMatrix();
                    const Math::Vector4 worldPos = worldMatrix * Math::Vector4(
                        localPosition.x, localPosition.y, localPosition.z, 1.0f);
                    const Math::Vector4 worldVel = worldMatrix * Math::Vector4(
                        localVelocity.x, localVelocity.y, localVelocity.z, 0.0f);
                    particle.position = Math::Vector3(worldPos.x, worldPos.y, worldPos.z);
                    particle.velocity = Math::Vector3(worldVel.x, worldVel.y, worldVel.z);
                } else {
                    particle.position = localPosition;
                    particle.velocity = localVelocity;
                }
            } else {
                particle.position = localPosition;
                particle.velocity = localVelocity;
            }

            particle.lifetime = std::max(startLifetime, kMinLifetime);
            particle.age = 0.0f;
            particle.size = startSize;
            particle.color = startColor;
            if (UsesTextureSheet()) {
                particle.animationOffset = RandomRange(0.0f, static_cast<float>(GetEffectiveFrameCount()));
            } else {
                particle.animationOffset = 0.0f;
            }
            activeSlots.push_back(slot);
            ++totalEmitted;
            ++activeParticleCount;
        }

        // Emits new particles, integrates gravity/velocity, lerps size/color, then uploads GPU data.
        void ParticleSystem::Tick(float deltaTime)
        {
            if (!playing || paused || deltaTime <= 0.0f) {
                return;
            }

            emissionAccumulator += deltaTime * emissionRate;
            const int particlesToSpawn = static_cast<int>(emissionAccumulator);
            emissionAccumulator -= static_cast<float>(particlesToSpawn);

            for (int i = 0; i < particlesToSpawn; ++i) {
                SpawnParticle();
            }

            Math::Vector3 simulationGravity = gravity;
            if (!worldSimulation && GetOwner()) {
                const Math::Vector4 localGravity4 =
                    GetOwner()->GetWorldMatrix().Inverse() *
                    Math::Vector4(gravity.x, gravity.y, gravity.z, 0.0f);
                simulationGravity = Math::Vector3(localGravity4.x, localGravity4.y, localGravity4.z);
            }

            for (std::size_t a = 0; a < activeSlots.size();) {
                const int slot = activeSlots[a];
                Rendering::Particle& particle = particles[static_cast<std::size_t>(slot)];

                particle.age += deltaTime;
                if (!particle.IsAlive()) {
                    freeSlots.push_back(slot);
                    --activeParticleCount;
                    activeSlots[a] = activeSlots.back();
                    activeSlots.pop_back();
                    continue;
                }

                particle.velocity += simulationGravity * deltaTime;
                particle.position += particle.velocity * deltaTime;

                const float t = Math::Clamp01(particle.age / particle.lifetime);
                particle.size = Math::Lerp(startSize, endSize, t);
                particle.color.r = Math::Lerp(startColor.r, endColor.r, t);
                particle.color.g = Math::Lerp(startColor.g, endColor.g, t);
                particle.color.b = Math::Lerp(startColor.b, endColor.b, t);
                particle.color.a = Math::Lerp(startColor.a, endColor.a, t);
                ++a;
            }

            if (!loop && totalEmitted > 0 && activeParticleCount == 0) {
                playing = false;
                hasCompletedPlayback = true;
            }

            UploadInstanceBuffer();
        }

        void ParticleSystem::TryDestroyOwnerWhenFinished()
        {
            if (loop || !destroyOwnerWhenFinished || destroyOwnerTriggered || !hasCompletedPlayback) {
                return;
            }

            GameObject* owner = GetOwner();
            if (!owner) {
                return;
            }

            Scene* scene = SceneManager::GetInstance().GetActiveScene();
            if (!scene || !scene->IsLifecycleComplete()) {
                return;
            }

            destroyOwnerTriggered = true;
            ObjectPool::GetInstance().Release(owner);
        }

        // Packs alive particles into instanceData and uploads them to the per-emitter instance VBO.
        void ParticleSystem::UploadInstanceBuffer()
        {
            activeInstanceCount = 0;

            // Hoist the owner's world matrix out of the per-particle loop for local-space emitters.
            const bool transformToWorld = !worldSimulation && GetOwner() != nullptr;
            const Math::Matrix4 worldMatrix =
                transformToWorld ? GetOwner()->GetWorldMatrix() : Math::Matrix4();

            for (const int slot : activeSlots) {
                const Rendering::Particle& particle = particles[static_cast<std::size_t>(slot)];

                Math::Vector3 worldPosition;
                if (transformToWorld) {
                    const Math::Vector4 transformed = worldMatrix * Math::Vector4(
                        particle.position.x, particle.position.y, particle.position.z, 1.0f);
                    worldPosition = Math::Vector3(transformed.x, transformed.y, transformed.z);
                } else {
                    worldPosition = particle.position;
                }

                ParticleInstanceData& instance = instanceData[static_cast<std::size_t>(activeInstanceCount)];
                instance.position = worldPosition;
                instance.color = Math::Vector4(particle.color.r, particle.color.g, particle.color.b, particle.color.a);
                instance.size = std::max(particle.size, kMinParticleSize);
                if (UsesTextureSheet() && textureSheetFramesPerSecond > 0.0f) {
                    const int frameCount = GetEffectiveFrameCount();
                    const float animatedFrame = particle.animationOffset + particle.age * textureSheetFramesPerSecond;
                    const int frameIndex = static_cast<int>(animatedFrame) % frameCount;
                    instance.frame = static_cast<float>(frameIndex);
                } else {
                    instance.frame = 0.0f;
                }
                ++activeInstanceCount;
            }

            if (!EnsureRenderResources()) {
                return;
            }

            // Replace the instance buffer contents for this frame (position, color, size per particle).
            auto& device = Device();
            if (activeInstanceCount > 0) {
                device.SetArrayBufferData(
                    instanceVbo,
                    instanceData.data(),
                    static_cast<std::size_t>(activeInstanceCount * sizeof(ParticleInstanceData)),
                    Rendering::RHI::BufferUsage::Dynamic);
            }
        }

        // Loads the particle shader, shared quad VAO, and wires instance attributes 2-4 with divisor 1.
        bool ParticleSystem::EnsureRenderResources()
        {
            if (!shader) {
                shader = Core::ResourceManager::GetInstance().GetShader("particle");
                if (!shader) {
                    shader = Core::ResourceManager::GetInstance().LoadShader(
                        "particle",
                        "Default/Shaders/particle.vert",
                        "Default/Shaders/particle.frag");
                }
            }

            if (!shader || !EnsureSharedQuadResources()) {
                return false;
            }

            if (instanceVbo == Rendering::RHI::kInvalidGpuId) {
                auto& device = Device();
                instanceVbo = device.CreateBuffer();

                device.BindVertexArray(g_particleShared.quadVao);
                device.SetArrayBufferData(
                    instanceVbo,
                    nullptr,
                    sizeof(ParticleInstanceData),
                    Rendering::RHI::BufferUsage::Dynamic);

                const int stride = static_cast<int>(sizeof(ParticleInstanceData));
                device.EnableVertexAttribFloat(2, 3, stride, offsetof(ParticleInstanceData, position));
                device.EnableVertexAttribFloat(3, 4, stride, offsetof(ParticleInstanceData, color));
                device.EnableVertexAttribFloat(4, 1, stride, offsetof(ParticleInstanceData, size));
                device.EnableVertexAttribFloat(5, 1, stride, offsetof(ParticleInstanceData, frame));

                device.SetVertexAttribDivisor(2, 1);
                device.SetVertexAttribDivisor(3, 1);
                device.SetVertexAttribDivisor(4, 1);
                device.SetVertexAttribDivisor(5, 1);

                device.UnbindVertexArray();

                ++g_particleShared.refCount;
            }

            return true;
        }

        // Deletes this emitter's instance VBO and drops the shared quad ref-count.
        void ParticleSystem::ReleaseRenderResources()
        {
            if (!Rendering::RHI::RenderDevice::HasDevice()) {
                instanceVbo = Rendering::RHI::kInvalidGpuId;
                g_particleShared.quadVao = Rendering::RHI::kInvalidGpuId;
                g_particleShared.quadVbo = Rendering::RHI::kInvalidGpuId;
                g_particleShared.refCount = 0;
                shader = nullptr;
                return;
            }

            if (instanceVbo != Rendering::RHI::kInvalidGpuId) {
                Device().DestroyBuffer(instanceVbo);
                instanceVbo = Rendering::RHI::kInvalidGpuId;

                if (g_particleShared.refCount > 0) {
                    --g_particleShared.refCount;
                }
            }

            ReleaseSharedQuadResources();
            shader = nullptr;
        }

        void ParticleSystem::DrawInstances()
        {
            auto& device = Device();
            device.BindVertexArray(g_particleShared.quadVao);
            device.BindArrayBuffer(instanceVbo);

            const int stride = static_cast<int>(sizeof(ParticleInstanceData));
            device.EnableVertexAttribFloat(2, 3, stride, offsetof(ParticleInstanceData, position));
            device.EnableVertexAttribFloat(3, 4, stride, offsetof(ParticleInstanceData, color));
            device.EnableVertexAttribFloat(4, 1, stride, offsetof(ParticleInstanceData, size));
            device.EnableVertexAttribFloat(5, 1, stride, offsetof(ParticleInstanceData, frame));
            // Re-assert instance rate after Enable* (OpenGL keeps divisor; Vulkan RHI must too).
            device.SetVertexAttribDivisor(2, 1);
            device.SetVertexAttribDivisor(3, 1);
            device.SetVertexAttribDivisor(4, 1);
            device.SetVertexAttribDivisor(5, 1);

            device.DrawArraysInstanced(
                Rendering::RHI::PrimitiveTopology::Triangles,
                0,
                6,
                activeInstanceCount);
            device.UnbindVertexArray();
        }

        // Renders billboard particles in two passes: depth silhouette, then alpha-blended color.
        void ParticleSystem::Render(Rendering::Camera* camera)
        {
            if (!isEnabled || !visible || !camera || activeInstanceCount <= 0) {
                return;
            }

            if (!GetOwner() || !GetOwner()->IsActiveInHierarchy()) {
                return;
            }

            if (!EnsureRenderResources()) {
                return;
            }

            auto& device = Device();
            device.SetDepthTest(true);
            device.SetCullFace(false);
            device.SetDepthFunc(Rendering::RHI::DepthFunc::Less);

            shader->Bind();
            Rendering::CameraUBO::GetInstance().Bind();
            Rendering::FogUniforms::Apply(shader);
            shader->SetBool("uHasTexture", textureRef != nullptr);
            const bool sheetEnabled = UsesTextureSheet();
            shader->SetBool("uSheetEnabled", sheetEnabled);
            shader->SetInt("uSheetColumns", textureSheetColumns);
            shader->SetInt("uSheetRows", textureSheetRows);
            shader->SetInt("uSheetFrameCount", GetEffectiveFrameCount());

            if (textureRef) {
                textureRef->Bind(0);
                shader->SetInt("uDiffuse", 0);
            }

            const bool additive = (blendMode == Rendering::ParticleBlendMode::Additive);

            if (!additive) {
                device.SetColorMask(false, false, false, false);
                device.SetDepthWrite(true);
                device.SetBlend(false);
                DrawInstances();
            }

            device.SetColorMask(true, true, true, true);
            device.SetDepthWrite(false);
            device.SetDepthFunc(Rendering::RHI::DepthFunc::LEqual);
            device.SetBlend(true);
            if (additive) {
                device.SetBlendFuncSeparate(
                    Rendering::RHI::BlendFactor::SrcAlpha,
                    Rendering::RHI::BlendFactor::One,
                    Rendering::RHI::BlendFactor::SrcAlpha,
                    Rendering::RHI::BlendFactor::One);
            } else {
                device.SetBlendFuncSeparate(
                    Rendering::RHI::BlendFactor::SrcAlpha,
                    Rendering::RHI::BlendFactor::OneMinusSrcAlpha,
                    Rendering::RHI::BlendFactor::SrcAlpha,
                    Rendering::RHI::BlendFactor::OneMinusSrcAlpha);
            }
            DrawInstances();

            if (textureRef) {
                textureRef->Unbind();
            }
            shader->Unbind();

            device.SetDepthFunc(Rendering::RHI::DepthFunc::Less);
            device.SetColorMask(true, true, true, true);
            device.SetDepthWrite(true);
            device.SetBlend(false);
            device.SetCullFace(true);
            device.SetDepthTest(true);
        }

        int ParticleSystem::GetEffectiveFrameCount() const
        {
            const int gridFrames = std::max(1, textureSheetColumns * textureSheetRows);
            return std::clamp(textureSheetFrameCount, 1, gridFrames);
        }

        bool ParticleSystem::UsesTextureSheet() const
        {
            return textureSheetColumns > 1 || textureSheetRows > 1 || GetEffectiveFrameCount() > 1;
        }

    }
}
