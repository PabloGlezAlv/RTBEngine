#include "ParticleSystem.h"

#include "GameObject.h"
#include "Scene.h"
#include "../Core/ResourceManager.h"
#include "../Rendering/Camera.h"
#include "../Rendering/Shader.h"
#include "../Rendering/Texture.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>

namespace RTBEngine {
    namespace ECS {

        namespace {
            constexpr float kMinParticleSize = 0.001f;
            constexpr float kMinLifetime = 0.05f;
            constexpr float kMinEmissionRate = 0.0f;
            constexpr int kMinMaxParticles = 1;
            constexpr int kMaxMaxParticles = 8192;

            struct QuadVertex {
                Math::Vector2 corner;
                Math::Vector2 uv;
            };

            struct ParticleSharedResources {
                GLuint quadVao = 0;
                GLuint quadVbo = 0;
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

            bool EnsureSharedQuadResources()
            {
                if (g_particleShared.quadVao != 0 && g_particleShared.quadVbo != 0) {
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

                glGenVertexArrays(1, &g_particleShared.quadVao);
                glGenBuffers(1, &g_particleShared.quadVbo);

                glBindVertexArray(g_particleShared.quadVao);
                glBindBuffer(GL_ARRAY_BUFFER, g_particleShared.quadVbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(QuadVertex), (void*)0);
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(QuadVertex), (void*)offsetof(QuadVertex, uv));

                glBindVertexArray(0);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                return g_particleShared.quadVao != 0;
            }

            void ReleaseSharedQuadResources()
            {
                if (g_particleShared.refCount > 0) {
                    return;
                }

                if (g_particleShared.quadVao != 0) {
                    glDeleteVertexArrays(1, &g_particleShared.quadVao);
                    g_particleShared.quadVao = 0;
                }
                if (g_particleShared.quadVbo != 0) {
                    glDeleteBuffers(1, &g_particleShared.quadVbo);
                    g_particleShared.quadVbo = 0;
                }
            }

            Rendering::Texture* CreateDefaultParticleTexture()
            {
                constexpr int size = 64;
                std::vector<unsigned char> pixels(static_cast<std::size_t>(size * size * 4));

                const float center = (static_cast<float>(size) - 1.0f) * 0.5f;
                const float radius = center;

                for (int y = 0; y < size; ++y) {
                    for (int x = 0; x < size; ++x) {
                        const float dx = static_cast<float>(x) - center;
                        const float dy = static_cast<float>(y) - center;
                        const float dist = std::sqrt(dx * dx + dy * dy) / radius;
                        const float alpha = std::clamp(1.0f - dist, 0.0f, 1.0f);
                        const float softAlpha = alpha * alpha;

                        const std::size_t index = static_cast<std::size_t>((y * size + x) * 4);
                        pixels[index + 0] = 255;
                        pixels[index + 1] = 255;
                        pixels[index + 2] = 255;
                        pixels[index + 3] = static_cast<unsigned char>(softAlpha * 255.0f);
                    }
                }

                auto* texture = new Rendering::Texture();
                if (!texture->LoadFromMemory(pixels.data(), size, size, 4)) {
                    delete texture;
                    return nullptr;
                }

                texture->SetFilter(Rendering::TextureFilter::Linear, Rendering::TextureFilter::Linear);
                texture->SetWrap(Rendering::TextureWrap::ClampToEdge, Rendering::TextureWrap::ClampToEdge);
                Core::ResourceManager::GetInstance().RegisterTexture("__particle_default__", texture);
                return texture;
            }
        }

        using ThisClass = ParticleSystem;
        RTB_REGISTER_COMPONENT(ParticleSystem)
            RTB_PROPERTY_RANGE(maxParticles, 1, 8192)
            RTB_PROPERTY_RANGE(emissionRate, 0.0f, 1000.0f)
            RTB_PROPERTY_ENUM(emitterShape, "Point", "Sphere", "Cone", "Box")
            RTB_PROPERTY_RANGE(shapeRadius, 0.0f, 10.0f)
            RTB_PROPERTY_RANGE(coneAngle, 0.0f, 90.0f)
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
            RTB_PROPERTY_RANGE(burstCount, 1, 256)
        RTB_END_REGISTER(ParticleSystem)

        ParticleSystem::ParticleSystem()
            : Component()
        {
        }

        ParticleSystem::~ParticleSystem()
        {
            ReleaseRenderResources();
        }

        void ParticleSystem::OnAwake()
        {
            ResizePool();
        }

        void ParticleSystem::OnStart()
        {
            ApplyPlaybackSettings();
        }

        void ParticleSystem::OnUpdate(float deltaTime)
        {
            Tick(deltaTime);
        }

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
            ResizePool();
            ApplyPlaybackSettings();
        }

        void ParticleSystem::OnDestroy()
        {
            ReleaseRenderResources();
        }

        void ParticleSystem::KillAllParticles()
        {
            for (Rendering::Particle& particle : particles) {
                particle.age = 0.0f;
                particle.lifetime = 0.0f;
            }
            activeInstanceCount = 0;
        }

        void ParticleSystem::ApplyPlaybackSettings()
        {
            if (playOnAwake && !userStopped) {
                Play();
            }
        }

        void ParticleSystem::ResizePool()
        {
            const int clampedMax = std::clamp(maxParticles, kMinMaxParticles, kMaxMaxParticles);
            maxParticles = clampedMax;

            const std::size_t previousSize = particles.size();
            particles.resize(static_cast<std::size_t>(clampedMax));
            instanceData.resize(static_cast<std::size_t>(clampedMax));

            for (std::size_t i = previousSize; i < particles.size(); ++i) {
                particles[i].age = 0.0f;
                particles[i].lifetime = 0.0f;
            }
        }

        void ParticleSystem::Play()
        {
            userStopped = false;
            playing = true;
            paused = false;
        }

        void ParticleSystem::Stop()
        {
            userStopped = true;
            playing = false;
            paused = false;
            emissionAccumulator = 0.0f;
            totalEmitted = 0;
            KillAllParticles();
            UploadInstanceBuffer();
        }

        void ParticleSystem::Pause()
        {
            if (playing) {
                paused = true;
            }
        }

        void ParticleSystem::Emit(int count)
        {
            if (count <= 0) {
                return;
            }

            playing = true;
            paused = false;

            for (int i = 0; i < count; ++i) {
                SpawnParticle();
            }
        }

        int ParticleSystem::GetActiveParticleCount() const
        {
            int count = 0;
            for (const Rendering::Particle& particle : particles) {
                if (particle.IsAlive()) {
                    ++count;
                }
            }
            return count;
        }

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

        int ParticleSystem::FindDeadParticleIndex() const
        {
            for (int i = 0; i < static_cast<int>(particles.size()); ++i) {
                if (!particles[static_cast<std::size_t>(i)].IsAlive()) {
                    return i;
                }
            }
            return -1;
        }

        Math::Vector3 ParticleSystem::SampleConeDirection() const
        {
            const float halfAngleRad = std::max(coneAngle, 0.0f) * 0.5f * 3.1415926535f / 180.0f;
            const float cosHalfAngle = std::cos(halfAngleRad);

            // coneAngle == 0 -> emit exactly along local +Y (transformed to world on spawn).
            if (cosHalfAngle >= 1.0f - 1e-6f) {
                return Math::Vector3(0.0f, 1.0f, 0.0f);
            }

            // Uniform random direction inside the cone aperture around +Y.
            const float cosTheta = RandomRange(cosHalfAngle, 1.0f);
            const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
            const float phi = RandomRange(0.0f, 6.28318530718f);
            return Math::Vector3(sinTheta * std::cos(phi), cosTheta, sinTheta * std::sin(phi));
        }

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

            default:
                return Math::Vector3::Zero();
            }
        }

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

            default:
                return RandomUnitVector().Normalized();
            }
        }

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

        void ParticleSystem::SpawnParticle()
        {
            const int slot = FindDeadParticleIndex();
            if (slot < 0) {
                return;
            }

            Rendering::Particle& particle = particles[static_cast<std::size_t>(slot)];
            const Math::Vector3 localPosition = SampleSpawnPosition();
            const Math::Vector3 localVelocity = SampleSpawnDirection() * startSpeed;

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
            ++totalEmitted;
        }

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

            for (Rendering::Particle& particle : particles) {
                if (!particle.IsAlive()) {
                    continue;
                }

                particle.age += deltaTime;
                if (!particle.IsAlive()) {
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
            }

            if (!loop && totalEmitted > 0 && GetActiveParticleCount() == 0) {
                playing = false;
            }

            UploadInstanceBuffer();
        }

        void ParticleSystem::UploadInstanceBuffer()
        {
            activeInstanceCount = 0;

            for (const Rendering::Particle& particle : particles) {
                if (!particle.IsAlive()) {
                    continue;
                }

                ParticleInstanceData& instance = instanceData[static_cast<std::size_t>(activeInstanceCount)];
                instance.position = ToWorldPosition(particle.position);
                instance.color = Math::Vector4(particle.color.r, particle.color.g, particle.color.b, particle.color.a);
                instance.size = std::max(particle.size, kMinParticleSize);
                ++activeInstanceCount;
            }

            if (!EnsureRenderResources()) {
                return;
            }

            glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
            if (activeInstanceCount > 0) {
                glBufferData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(activeInstanceCount * sizeof(ParticleInstanceData)),
                    instanceData.data(),
                    GL_DYNAMIC_DRAW);
            }
        }

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

            if (instanceVbo == 0) {
                glGenBuffers(1, &instanceVbo);

                glBindVertexArray(g_particleShared.quadVao);
                glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof(ParticleInstanceData), nullptr, GL_DYNAMIC_DRAW);

                const GLsizei stride = static_cast<GLsizei>(sizeof(ParticleInstanceData));
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
                glEnableVertexAttribArray(3);
                glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ParticleInstanceData, color));
                glEnableVertexAttribArray(4);
                glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ParticleInstanceData, size));

                glVertexAttribDivisor(2, 1);
                glVertexAttribDivisor(3, 1);
                glVertexAttribDivisor(4, 1);

                glBindVertexArray(0);
                glBindBuffer(GL_ARRAY_BUFFER, 0);

                ++g_particleShared.refCount;
            }

            if (!runtimeTexture) {
                runtimeTexture = textureRef;
                if (!runtimeTexture) {
                    runtimeTexture = Core::ResourceManager::GetInstance().GetTexture("__particle_default__");
                    if (!runtimeTexture) {
                        runtimeTexture = CreateDefaultParticleTexture();
                    }
                }
            }

            return true;
        }

        void ParticleSystem::ReleaseRenderResources()
        {
            if (instanceVbo != 0) {
                glDeleteBuffers(1, &instanceVbo);
                instanceVbo = 0;

                if (g_particleShared.refCount > 0) {
                    --g_particleShared.refCount;
                }
            }

            ReleaseSharedQuadResources();
            shader = nullptr;
            runtimeTexture = nullptr;
        }

        void ParticleSystem::DrawInstances()
        {
            glBindVertexArray(g_particleShared.quadVao);
            glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
            const GLsizei stride = static_cast<GLsizei>(sizeof(ParticleInstanceData));
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
            glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ParticleInstanceData, color));
            glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ParticleInstanceData, size));
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, activeInstanceCount);
            glBindVertexArray(0);
        }

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

            runtimeTexture = textureRef ? textureRef : runtimeTexture;
            if (!runtimeTexture) {
                runtimeTexture = Core::ResourceManager::GetInstance().GetTexture("__particle_default__");
                if (!runtimeTexture) {
                    runtimeTexture = CreateDefaultParticleTexture();
                }
            }

            const GLboolean wasBlendEnabled = glIsEnabled(GL_BLEND);
            const GLboolean wasDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
            const GLboolean wasCullFaceEnabled = glIsEnabled(GL_CULL_FACE);
            GLboolean wasDepthMask = GL_TRUE;
            GLboolean previousColorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
            GLint previousDepthFunc = GL_LESS;
            glGetBooleanv(GL_DEPTH_WRITEMASK, &wasDepthMask);
            glGetBooleanv(GL_COLOR_WRITEMASK, previousColorMask);
            glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);

            glEnable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glDepthFunc(GL_LESS);

            shader->Bind();
            shader->SetMatrix4("uView", camera->GetViewMatrix());
            shader->SetMatrix4("uProjection", camera->GetProjectionMatrix());
            shader->SetVector3("uCameraRight", camera->GetRight());
            shader->SetVector3("uCameraUp", camera->GetUp());
            shader->SetBool("uHasTexture", runtimeTexture != nullptr);

            if (runtimeTexture) {
                runtimeTexture->Bind(0);
                shader->SetInt("uDiffuse", 0);
            }

            // Pass 1: write depth for visible particle fragments so opaque meshes occlude correctly.
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            DrawInstances();

            // Pass 2: draw color with depth test against meshes and the particle depth silhouette.
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glDepthMask(GL_FALSE);
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            DrawInstances();

            if (runtimeTexture) {
                runtimeTexture->Unbind();
            }
            shader->Unbind();

            glDepthFunc(previousDepthFunc);
            glColorMask(
                previousColorMask[0],
                previousColorMask[1],
                previousColorMask[2],
                previousColorMask[3]);
            glDepthMask(wasDepthMask ? GL_TRUE : GL_FALSE);
            if (wasCullFaceEnabled) {
                glEnable(GL_CULL_FACE);
            } else {
                glDisable(GL_CULL_FACE);
            }
            if (wasDepthTestEnabled) {
                glEnable(GL_DEPTH_TEST);
            } else {
                glDisable(GL_DEPTH_TEST);
            }
            if (wasBlendEnabled) {
                glEnable(GL_BLEND);
            } else {
                glDisable(GL_BLEND);
            }
        }

    }
}
