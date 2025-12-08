#pragma once
#include "Component.h"
#include "../Rendering/Mesh.h"
#include "../Rendering/Material.h"
#include "../Rendering/Camera.h"

namespace RTBEngine {
    namespace ECS {

        class MeshRenderer : public Component {
        public:
            MeshRenderer();
            ~MeshRenderer() override;

            void SetMesh(Rendering::Mesh* mesh);
            void SetMaterial(Rendering::Material* material);

            Rendering::Mesh* GetMesh() const { return mesh; }
            Rendering::Material* GetMaterial() const { return material; }

            void Render(Rendering::Camera* camera);

            const char* GetTypeName() const override { return "MeshRenderer"; }

        private:
            Rendering::Mesh* mesh;
            Rendering::Material* material;
        };

    }
}