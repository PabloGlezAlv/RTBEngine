#include "AnimatedBillboard.h"

#include "GameObject.h"
#include "Scene.h"

#include "../Core/ResourceManager.h"
#include "../Rendering/Camera.h"
#include "../Rendering/CameraUBO.h"
#include "../Rendering/Shader.h"
#include "../Rendering/Texture.h"
#include "../Rendering/RHI/RenderDevice.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace RTBEngine {
    namespace Scene {

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

            if (vao != Rendering::RHI::kInvalidGpuId && vbo != Rendering::RHI::kInvalidGpuId) {
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

            auto& device = Rendering::RHI::RenderDevice::Get();
            vao = device.CreateVertexArray();
            vbo = device.CreateBuffer();
            device.BindVertexArray(vao);
            device.SetArrayBufferData(
                vbo,
                vertices,
                sizeof(vertices),
                Rendering::RHI::BufferUsage::Static);
            device.EnableVertexAttribFloat(
                0, 2, static_cast<int>(sizeof(BillboardVertex)), offsetof(BillboardVertex, corner));
            device.EnableVertexAttribFloat(
                1, 2, static_cast<int>(sizeof(BillboardVertex)), offsetof(BillboardVertex, uv));
            device.UnbindVertexArray();
            return vao != Rendering::RHI::kInvalidGpuId;
        }

        void AnimatedBillboard::ReleaseRenderResources()
        {
            auto& device = Rendering::RHI::RenderDevice::Get();
            if (vbo != Rendering::RHI::kInvalidGpuId) {
                device.DestroyBuffer(vbo);
                vbo = Rendering::RHI::kInvalidGpuId;
            }
            if (vao != Rendering::RHI::kInvalidGpuId) {
                device.DestroyVertexArray(vao);
                vao = Rendering::RHI::kInvalidGpuId;
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

            auto& device = Rendering::RHI::RenderDevice::Get();
            device.SetDepthTest(true);
            device.SetCullFace(false);
            device.SetDepthWrite(false);
            device.SetDepthFunc(Rendering::RHI::DepthFunc::LEqual);
            device.SetBlend(true);
            if (blendMode == Rendering::ParticleBlendMode::Additive) {
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

            device.BindVertexArray(vao);
            device.DrawArrays(Rendering::RHI::PrimitiveTopology::Triangles, 0, 6);
            device.UnbindVertexArray();

            if (textureRef) {
                textureRef->Unbind();
            }
            shader->Unbind();

            device.SetDepthWrite(true);
            device.SetDepthFunc(Rendering::RHI::DepthFunc::Less);
            device.SetBlend(false);
            device.SetCullFace(true);
            device.SetDepthTest(true);
        }

    }
}
