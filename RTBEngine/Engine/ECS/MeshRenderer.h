#pragma once
#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Reflection/PropertyMacros.h"
#include "../Rendering/Mesh.h"
#include "../Rendering/Material.h"
#include "../Rendering/Camera.h"
#include <vector>
#include <memory>
#include <cstdint>

namespace RTBEngine {
	namespace Rendering {
		class Light;
	}
}

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

            void SetMesh(Rendering::Mesh* mesh);
            Rendering::Mesh* GetMesh() const { return mesh; }

            Rendering::Material* GetMaterial() const { return material.get(); }
            void SetMaterial(Rendering::Material* mat);

            void SetTexture(Rendering::Texture* tex);
            void SetShader(Rendering::Shader* shader);

            void Render(Rendering::Camera* camera, const std::vector<Rendering::Light*>& lights);

            //Render stats
            static void ResetRenderStats();
            static uint32_t GetDrawCallCount() { return drawCallCount; }
            static uint32_t GetTriangleCount() { return triangleCount; }

            virtual void OnAwake() override;
            virtual void OnUpdate(float deltaTime) override;
            virtual void OnValidate() override;

            // Reflected properties (Proxy)
            Rendering::Mesh* meshRef = nullptr;
            Rendering::Texture* textureRef = nullptr;
            Math::Vector4 colorRef = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            int meshIndex = 0;

            RTB_COMPONENT(MeshRenderer)

        private:
            Rendering::Mesh* mesh = nullptr;
            std::unique_ptr<Rendering::Material> material;

            void SyncProperties();

            static uint32_t drawCallCount;
            static uint32_t triangleCount;
        };
#pragma warning(pop)

    }
}
