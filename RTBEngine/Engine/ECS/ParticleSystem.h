#pragma once

#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Rendering/ParticleTypes.h"
#include "../Reflection/PropertyMacros.h"
#include "../Math/Color.h"
#include "../Math/Vectors/Vector3.h"

#include <GL/glew.h>
#include <cstddef>
#include <vector>

namespace RTBEngine {
    namespace Rendering {
        class Camera;
        class Shader;
        class Texture;
    }
    namespace ECS {
        class Scene;
    }
}

namespace RTBEngine {
    namespace ECS {

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API ParticleSystem : public Component {
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

            void Tick(float deltaTime);
            void Render(Rendering::Camera* camera);

            void Play();
            void Stop();
            void Pause();
            bool IsPlaying() const { return playing; }
            bool IsPaused() const { return paused; }

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

            int burstCount = 10;

            RTB_COMPONENT(ParticleSystem)

        private:
            struct ParticleInstanceData {
                Math::Vector3 position;
                Math::Vector4 color;
                float size = 0.0f;
            };

            void ResizePool();
            void SpawnParticle();
            int FindDeadParticleIndex() const;
            Math::Vector3 SampleConeDirection() const;
            Math::Vector3 SampleSpawnPosition() const;
            Math::Vector3 SampleSpawnDirection() const;
            Math::Vector3 ToWorldPosition(const Math::Vector3& localOrWorld) const;
            Math::Vector3 ToWorldDirection(const Math::Vector3& localOrWorld) const;
            void UploadInstanceBuffer();
            bool EnsureRenderResources();
            void ReleaseRenderResources();
            void KillAllParticles();
            void ApplyPlaybackSettings();
            void DrawInstances();

            std::vector<Rendering::Particle> particles;
            std::vector<ParticleInstanceData> instanceData;

            float emissionAccumulator = 0.0f;
            bool playing = false;
            bool paused = false;
            bool userStopped = false;

            GLuint instanceVbo = 0;
            Rendering::Shader* shader = nullptr;
            Rendering::Texture* runtimeTexture = nullptr;
            int activeInstanceCount = 0;
            int totalEmitted = 0;
        };
#pragma warning(pop)

    }
}
