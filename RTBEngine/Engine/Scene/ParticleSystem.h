#pragma once

#include "../RTBEngineAPI.h"
#include "Component.h"
#include "IPoolable.h"
#include "../Rendering/ParticleTypes.h"
#include "../Rendering/RHI/RenderTypes.h"
#include "../Reflection/PropertyMacros.h"
#include "../Math/Color.h"
#include "../Math/Vectors/Vector3.h"
#include <vector>

namespace RTBEngine {
    namespace Rendering {
        class Camera;
        class Shader;
        class Texture;
    }
    namespace Scene {
        class Scene;
    }
}

namespace RTBEngine {
    namespace Scene {

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API ParticleSystem : public Component, public IPoolable {
        public:
            ParticleSystem();
            ~ParticleSystem() override;

            ParticleSystem(const ParticleSystem&) = delete;
            ParticleSystem& operator=(const ParticleSystem&) = delete;

            void OnAwake() override;
            void OnStart() override;
            void OnUpdate(float deltaTime) override;
            void OnValidate() override;
            void OnDestroy() override;

            void OnPoolAcquire() override;
            void OnPoolRelease() override;

            void Tick(float deltaTime);
            void Render(Rendering::Camera* camera);

            void Play();
            void Stop();
            void Pause();
            void Restart();
            bool IsPlaying() const { return playing; }
            bool IsPaused() const { return paused; }
            bool HasCompletedPlayback() const { return hasCompletedPlayback; }

            void Emit(int count);
            int GetActiveParticleCount() const;

            static void TickScenePreview(Scene* scene, float deltaTime);

            int maxParticles = 256;
            float emissionRate = 40.0f;
            Rendering::ParticleEmitterShape emitterShape = Rendering::ParticleEmitterShape::Cone;
            float shapeRadius = 0.5f;
            float coneAngle = 25.0f;
            Math::Vector3 boxSize = Math::Vector3(1.0f, 1.0f, 1.0f);

            float startLifetime = 1.5f;
            float startSpeed = 2.0f;
            float startSize = 0.3f;
            float endSize = 0.05f;
            Math::Color startColor = Math::Color::White();
            Math::Color endColor = Math::Color(1.0f, 1.0f, 1.0f, 0.0f);

            Math::Vector3 gravity = Math::Vector3(0.0f, -2.0f, 0.0f);
            bool worldSimulation = true;

            Rendering::Texture* textureRef = nullptr;
            bool visible = true;

            bool loop = true;
            bool playOnAwake = true;
            bool simulateInEditMode = true;
            bool destroyOwnerWhenFinished = false;

            int burstCount = 10;

            int textureSheetColumns = 1;
            int textureSheetRows = 1;
            int textureSheetFrameCount = 1;
            float textureSheetFramesPerSecond = 12.0f;
            Rendering::ParticleBlendMode blendMode = Rendering::ParticleBlendMode::Alpha;

            RTB_COMPONENT(ParticleSystem)

        private:
            struct ParticleInstanceData {
                Math::Vector3 position;
                Math::Vector4 color;
                float size = 0.0f;
                float frame = 0.0f;
            };

            void ResizePool();
            void RebuildFreeSlots();
            void SpawnParticle();
            Math::Vector3 SampleConeDirection() const;
            Math::Vector3 SampleSpawnPosition() const;
            Math::Vector3 SampleSpawnDirection() const;
            Math::Vector3 ToWorldPosition(const Math::Vector3& localOrWorld) const;
            Math::Vector3 ToWorldDirection(const Math::Vector3& localOrWorld) const;
            void UploadInstanceBuffer();
            bool EnsureRenderResources();
            void ReleaseRenderResources();
            void KillAllParticles();
            void ClearSimulation();
            void BeginPlayback(bool emitBurstIfRateZero);
            void ApplyPlaybackSettings();
            void TryDestroyOwnerWhenFinished();
            void DrawInstances();
            int GetEffectiveFrameCount() const;
            bool UsesTextureSheet() const;
            bool IsBurstEmitter() const;

            std::vector<Rendering::Particle> particles;
            std::vector<ParticleInstanceData> instanceData;
            std::vector<int> freeSlots;
            // Indices of currently-alive particles, so Tick/Upload iterate only live slots
            // instead of scanning the whole pool (which can be up to 8192 entries).
            std::vector<int> activeSlots;

            float emissionAccumulator = 0.0f;
            bool playing = false;
            bool paused = false;
            bool userStopped = false;
            bool hasCompletedPlayback = false;

            Rendering::RHI::GpuId instanceVbo = Rendering::RHI::kInvalidGpuId;
            Rendering::Shader* shader = nullptr;
            int activeInstanceCount = 0;
            int activeParticleCount = 0;
            int totalEmitted = 0;
            bool destroyOwnerTriggered = false;
        };
#pragma warning(pop)

    }
}
