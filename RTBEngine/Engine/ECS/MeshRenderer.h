#pragma once
#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Reflection/PropertyMacros.h"
#include "../Rendering/Mesh.h"
#include "../Rendering/Material.h"
#include <vector>
#include <memory>
#include <cstdint>

namespace RTBEngine {
    namespace ECS {

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

            //Multi-mesh API
            void SetMeshes(const std::vector<Rendering::Mesh*>& meshList);
            void SetMaterialForMesh(int index, Rendering::Material* mat);
            const std::vector<Rendering::Mesh*>& GetMeshes() const { return meshes; }
            int GetMeshCount() const { return static_cast<int>(meshes.size()); }
            Rendering::Material* GetMaterialForMesh(int index) const;
            bool IsMultiMesh() const { return multiMesh; }
            void GetCombinedAABB(Math::Vector3& outMin, Math::Vector3& outMax) const;

            void Render();

            //Render stats
            static void ResetRenderStats();
            static uint32_t GetDrawCallCount() { return drawCallCount; }
            static uint32_t GetTriangleCount() { return triangleCount; }

            static uint32_t GetCulledObjectCount() { return culledObjectCount; }
            static void IncrementCulledCount() { culledObjectCount++; }

            virtual void OnAwake() override;
            virtual void OnUpdate(float deltaTime) override;
            virtual void OnValidate() override;

            // Reflected properties (Proxy)
            Rendering::Mesh* meshRef = nullptr;
            Rendering::Texture* textureRef = nullptr;
            Math::Vector4 colorRef = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
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

            void SyncProperties();
            void RenderMultiMesh();

            static uint32_t drawCallCount;
            static uint32_t culledObjectCount;
            static uint32_t triangleCount;
        };
#pragma warning(pop)

    }
}
