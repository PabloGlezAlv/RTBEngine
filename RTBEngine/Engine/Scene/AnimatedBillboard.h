#pragma once

#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Rendering/ParticleTypes.h"
#include "../Rendering/RHI/RenderTypes.h"
#include "../Reflection/PropertyMacros.h"
#include "../Math/Color.h"
#include "../Math/Vectors/Vector2.h"

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
        // Camera-facing quad with flipbook UVs. Used for flame body/core billboards.
        class RTB_API AnimatedBillboard : public Component {
        public:
            AnimatedBillboard();
            ~AnimatedBillboard() override;

            AnimatedBillboard(const AnimatedBillboard&) = delete;
            AnimatedBillboard& operator=(const AnimatedBillboard&) = delete;

            void OnAwake() override;
            void OnStart() override;
            void OnUpdate(float deltaTime) override;
            void OnValidate() override;
            void OnDestroy() override;

            void Tick(float deltaTime);
            void Render(Rendering::Camera* camera);

            static void TickScenePreview(Scene* scene, float deltaTime);

            Rendering::Texture* textureRef = nullptr;
            Math::Color color = Math::Color(1.0f, 0.85f, 0.35f, 1.0f);
            Math::Vector2 size = Math::Vector2(0.45f, 0.75f);
            float verticalOffset = 0.0f;

            int textureSheetColumns = 1;
            int textureSheetRows = 1;
            int textureSheetFrameCount = 1;
            float textureSheetFramesPerSecond = 12.0f;
            float animationOffset = 0.0f;

            Rendering::ParticleBlendMode blendMode = Rendering::ParticleBlendMode::Additive;
            bool visible = true;
            bool playOnAwake = true;
            bool simulateInEditMode = true;
            bool randomStartFrame = true;

            RTB_COMPONENT(AnimatedBillboard)

        private:
            bool EnsureRenderResources();
            void ReleaseRenderResources();
            int GetEffectiveFrameCount() const;
            bool UsesTextureSheet() const;
            float ResolveCurrentFrame() const;

            float animationTime = 0.0f;
            bool playing = false;
            Rendering::RHI::GpuId vao = Rendering::RHI::kInvalidGpuId;
            Rendering::RHI::GpuId vbo = Rendering::RHI::kInvalidGpuId;
            Rendering::Shader* shader = nullptr;
        };
#pragma warning(pop)

    }
}
