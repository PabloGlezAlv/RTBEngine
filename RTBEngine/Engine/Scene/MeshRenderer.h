#pragma once
#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Reflection/PropertyMacros.h"
#include "../Rendering/Mesh.h"
#include "../Rendering/Material.h"
#include "../Rendering/ShaderProperties.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <cstddef>

namespace RTBEngine {
    namespace Animation {
        class Animator;
    }

    namespace Scene {

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API MeshRenderer : public Component {
        public:
            MeshRenderer();
            ~MeshRenderer() override;

            MeshRenderer(const MeshRenderer&) = delete;
            MeshRenderer& operator=(const MeshRenderer&) = delete;

            //Single-mesh API
            void SetMesh(Rendering::Mesh* mesh);
            Rendering::Mesh* GetMesh() const { return mesh; }

            Rendering::Material* GetMaterial() const { return material.get(); }
            void SetMaterial(Rendering::Material* mat);

            void SetTexture(Rendering::Texture* tex);
            void SetShader(Rendering::Shader* shader);

            // GPU instancing: when instanceCount > 0, RenderInstanced() draws N copies without N GameObjects.
            void SetInstances(const Math::Matrix4* matrices, std::size_t count);
            void SetInstanceColors(const Math::Vector4* colors, std::size_t count);
            void ClearInstances();
            std::size_t GetInstanceCount() const { return instanceMatrices.size(); }
            bool HasInstanceColors() const { return useInstanceColors && !instanceColors.empty(); }

            //Multi-mesh API
            void SetMeshes(const std::vector<Rendering::Mesh*>& meshList);
            void SetMaterialForMesh(int index, Rendering::Material* mat);
            const std::vector<Rendering::Mesh*>& GetMeshes() const { return meshes; }
            int GetMeshCount() const { return static_cast<int>(meshes.size()); }
            Rendering::Material* GetMaterialForMesh(int index) const;
            bool IsMultiMesh() const { return multiMesh; }
            void GetCombinedAABB(Math::Vector3& outMin, Math::Vector3& outMax) const;

            float GetOcclusionFadeAlpha() const { return occlusionFadeAlpha; }
            void SetOcclusionFadeAlpha(float alpha);

            void Render();
            void RenderInstanced();
            void RenderDraw(Rendering::Mesh* drawMesh, Rendering::Material* drawMaterial);

            // Animator that skins this renderer, searched up the hierarchy and cached until the
            // scene hierarchy changes. Used by both the geometry pass and the shadow pass.
            Animation::Animator* GetActiveAnimator();

            //Render stats
            static void ResetRenderStats();
            static uint32_t GetDrawCallCount() { return drawCallCount; }
            static uint32_t GetTriangleCount() { return triangleCount; }

            static uint32_t GetCulledObjectCount() { return culledObjectCount; }
            static void IncrementCulledCount() { culledObjectCount++; }

            // One instanced draw call covering instanceCount meshes; keeps debug stats accurate.
            static void AddInstancedDrawStats(uint32_t indexCount, uint32_t instanceCount);

            virtual void OnAwake() override;
            virtual void OnValidate() override;
            virtual void OnDestroy() override;

            void EnsureShaderOverrideCache();
            void PrepareForRender();

            // Reflected properties (Proxy)
            Rendering::Mesh* meshRef = nullptr;
            Rendering::Texture* textureRef = nullptr;
            Math::Vector4 colorRef = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            std::string shaderRef = "basic";
            std::string shaderPropertyOverrides;
            Rendering::ShaderPropertyOverrideCache shaderOverrideCache;
            int meshIndex = 0;
            bool multiMesh = false;

            RTB_COMPONENT(MeshRenderer)

        private:
            //Single-mesh state
            Rendering::Mesh* mesh = nullptr;
            std::unique_ptr<Rendering::Material> material;

            //Multi-mesh state
            std::vector<Rendering::Mesh*> meshes;
            std::vector<std::unique_ptr<Rendering::Material>> meshMaterials;

            // Explicit GPU instancing buffers (optional).
            std::vector<Math::Matrix4> instanceMatrices;
            std::vector<Math::Vector4> instanceColors;
            bool useInstanceColors = false;

            void SyncProperties();
            void SyncPropertiesIfDirty();
            void SyncMaterialColorIfNeeded();
            void RenderMultiMesh();

            float occlusionFadeAlpha = 1.0f;
            bool propertiesDirty = true;

            // Cached ancestor animator; invalidated when GameObject::GetHierarchyVersion() changes.
            Animation::Animator* cachedAnimator = nullptr;
            uint32_t cachedAnimatorHierarchyVersion = 0;
            bool animatorCacheValid = false;

            static uint32_t drawCallCount;
            static uint32_t culledObjectCount;
            static uint32_t triangleCount;
        };
#pragma warning(pop)

    }
}
