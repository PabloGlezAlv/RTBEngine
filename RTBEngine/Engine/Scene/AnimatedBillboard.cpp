#include "AnimatedBillboard.h"

#include "GameObject.h"
#include "Scene.h"

#include "../Core/ResourceManager.h"
#include "../Rendering/Camera.h"
#include "../Rendering/CameraUBO.h"
#include "../Rendering/Shader.h"
#include "../Rendering/Texture.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace RTBEngine {
    namespace ECS {

        namespace {
            struct BillboardVertex {
                Math::Vector2 corner;
                Math::Vector2 uv;
            };
        }

        using ThisClass = AnimatedBillboard;
        RTB_REGISTER_COMPONENT(AnimatedBillboard)
            RTB_PROPERTY_TEXTURE(textureRef)
            RTB_PROPERTY_COLOR(color)
            RTB_PROPERTY(size)
            RTB_PROPERTY(verticalOffset)
            RTB_PROPERTY_RANGE(textureSheetColumns, 1, 32)
            RTB_PROPERTY_RANGE(textureSheetRows, 1, 32)
            RTB_PROPERTY_RANGE(textureSheetFrameCount, 1, 1024)
            RTB_PROPERTY_RANGE(textureSheetFramesPerSecond, 0.0f, 120.0f)
            RTB_PROPERTY(animationOffset)
            RTB_PROPERTY_ENUM(blendMode, "Alpha", "Additive")
            RTB_PROPERTY(visible)
            RTB_PROPERTY(playOnAwake)
            RTB_PROPERTY(simulateInEditMode)
            RTB_PROPERTY(randomStartFrame)
        RTB_END_REGISTER(AnimatedBillboard)

        AnimatedBillboard::AnimatedBillboard()
            : Component()
        {
        }

        AnimatedBillboard::~AnimatedBillboard()
        {
            ReleaseRenderResources();
        }

        void AnimatedBillboard::OnAwake()
        {
            EnsureRenderResources();
        }

        void AnimatedBillboard::OnStart()
        {
            if (randomStartFrame) {
                animationOffset = static_cast<float>(std::rand() % 1000) * 0.01f;
            }
            if (playOnAwake) {
                playing = true;
            }
        }

        void AnimatedBillboard::OnUpdate(float deltaTime)
        {
            Tick(deltaTime);
        }

        void AnimatedBillboard::OnValidate()
        {
            textureSheetColumns = std::clamp(textureSheetColumns, 1, 32);
            textureSheetRows = std::clamp(textureSheetRows, 1, 32);
            textureSheetFrameCount = std::clamp(textureSheetFrameCount, 1, 1024);
            textureSheetFramesPerSecond = std::max(textureSheetFramesPerSecond, 0.0f);
            size.x = std::max(size.x, 0.01f);
            size.y = std::max(size.y, 0.01f);
            if (playOnAwake) {
                playing = true;
            }
            EnsureRenderResources();
        }

        void AnimatedBillboard::OnDestroy()
        {
            ReleaseRenderResources();
        }

        void AnimatedBillboard::Tick(float deltaTime)
        {
            if (!playing || deltaTime <= 0.0f) {
                return;
            }
            animationTime += deltaTime;
        }

        void AnimatedBillboard::TickScenePreview(Scene* scene, float deltaTime)
        {
            if (!scene || deltaTime <= 0.0f) {
                return;
            }

            for (const auto& gameObjectPtr : scene->GetGameObjects()) {
                GameObject* gameObject = gameObjectPtr.get();
                if (!gameObject || !gameObject->IsActiveInHierarchy()) {
                    continue;
                }

                AnimatedBillboard* billboard = gameObject->GetComponent<AnimatedBillboard>();
                if (!billboard || !billboard->IsEnabled()) {
                    continue;
                }
                if (!billboard->simulateInEditMode || !billboard->playing) {
                    continue;
                }
                billboard->Tick(deltaTime);
            }
        }

        int AnimatedBillboard::GetEffectiveFrameCount() const
        {
            const int gridFrames = std::max(1, textureSheetColumns * textureSheetRows);
            return std::clamp(textureSheetFrameCount, 1, gridFrames);
        }

        bool AnimatedBillboard::UsesTextureSheet() const
        {
            return textureSheetColumns > 1 || textureSheetRows > 1 || GetEffectiveFrameCount() > 1;
        }

        float AnimatedBillboard::ResolveCurrentFrame() const
        {
            if (!UsesTextureSheet() || textureSheetFramesPerSecond <= 0.0f) {
                return 0.0f;
            }
            const int frameCount = GetEffectiveFrameCount();
            const float animated = animationOffset + animationTime * textureSheetFramesPerSecond;
            const int frameIndex = static_cast<int>(animated) % frameCount;
            return static_cast<float>((frameIndex + frameCount) % frameCount);
        }

        bool AnimatedBillboard::EnsureRenderResources()
        {
            if (!shader) {
                shader = Core::ResourceManager::GetInstance().GetShader("billboard");
                if (!shader) {
                    shader = Core::ResourceManager::GetInstance().LoadShader(
                        "billboard",
                        "Default/Shaders/billboard.vert",
                        "Default/Shaders/billboard.frag");
                }
            }

            if (!shader) {
                return false;
            }

            if (vao != 0 && vbo != 0) {
                return true;
            }

            const BillboardVertex vertices[] = {
                { Math::Vector2(-0.5f, -0.5f), Math::Vector2(0.0f, 0.0f) },
                { Math::Vector2(0.5f, -0.5f), Math::Vector2(1.0f, 0.0f) },
                { Math::Vector2(0.5f, 0.5f), Math::Vector2(1.0f, 1.0f) },
                { Math::Vector2(-0.5f, -0.5f), Math::Vector2(0.0f, 0.0f) },
                { Math::Vector2(0.5f, 0.5f), Math::Vector2(1.0f, 1.0f) },
                { Math::Vector2(-0.5f, 0.5f), Math::Vector2(0.0f, 1.0f) },
            };

            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(BillboardVertex), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(BillboardVertex), (void*)offsetof(BillboardVertex, uv));
            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            return vao != 0;
        }

        void AnimatedBillboard::ReleaseRenderResources()
        {
            if (vbo != 0) {
                glDeleteBuffers(1, &vbo);
                vbo = 0;
            }
            if (vao != 0) {
                glDeleteVertexArrays(1, &vao);
                vao = 0;
            }
            shader = nullptr;
        }

        void AnimatedBillboard::Render(Rendering::Camera* camera)
        {
            if (!isEnabled || !visible || !camera) {
                return;
            }
            if (!GetOwner() || !GetOwner()->IsActiveInHierarchy()) {
                return;
            }
            if (!EnsureRenderResources()) {
                return;
            }

            const GLboolean wasBlendEnabled = glIsEnabled(GL_BLEND);
            const GLboolean wasDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
            const GLboolean wasCullFaceEnabled = glIsEnabled(GL_CULL_FACE);
            GLboolean wasDepthMask = GL_TRUE;
            glGetBooleanv(GL_DEPTH_WRITEMASK, &wasDepthMask);

            glEnable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glDepthMask(GL_FALSE);
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_BLEND);
            if (blendMode == Rendering::ParticleBlendMode::Additive) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            } else {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }

            const Math::Vector3 worldPosition = GetOwner()->GetWorldPosition();
            shader->Bind();
            Rendering::CameraUBO::GetInstance().Bind();
            shader->SetVector3("uWorldPosition", worldPosition);
            shader->SetVector2("uSize", size);
            shader->SetFloat("uVerticalOffset", verticalOffset);
            shader->SetVector4("uColor", Math::Vector4(color.r, color.g, color.b, color.a));
            shader->SetBool("uHasTexture", textureRef != nullptr);
            const bool sheetEnabled = UsesTextureSheet();
            shader->SetBool("uSheetEnabled", sheetEnabled);
            shader->SetInt("uSheetColumns", textureSheetColumns);
            shader->SetInt("uSheetRows", textureSheetRows);
            shader->SetInt("uSheetFrameCount", GetEffectiveFrameCount());
            shader->SetFloat("uFrame", ResolveCurrentFrame());

            if (textureRef) {
                textureRef->Bind(0);
                shader->SetInt("uDiffuse", 0);
            }

            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            if (textureRef) {
                textureRef->Unbind();
            }
            shader->Unbind();

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
