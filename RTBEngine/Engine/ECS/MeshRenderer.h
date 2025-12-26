#pragma once
#include "Component.h"
#include "../Rendering/Mesh.h"
#include "../Rendering/Material.h"
#include "../Rendering/Camera.h"
#include <vector>
#include <memory>

namespace RTBEngine {
	namespace Rendering {
		class Light;
	}
}

namespace RTBEngine {
    namespace ECS {

        class MeshRenderer : public Component {
        public:
            MeshRenderer();
            ~MeshRenderer() override;

            void SetMesh(Rendering::Mesh* mesh);
            void SetMeshes(const std::vector<Rendering::Mesh*>& meshes);

            Rendering::Mesh* GetMesh() const { return meshes.empty() ? nullptr : meshes[0]; }
            const std::vector<Rendering::Mesh*>& GetMeshes() const { return meshes; }
            Rendering::Material* GetMaterial() const { return material.get(); }

            void SetTexture(Rendering::Texture* tex);
            void SetShader(Rendering::Shader* shader);

            void Render(Rendering::Camera* camera, const std::vector<Rendering::Light*>& lights);

            const char* GetTypeName() const override { return "MeshRenderer"; }

        private:
            std::vector<Rendering::Mesh*> meshes;
            std::unique_ptr<Rendering::Material> material;
        };

    }
}