#pragma once
#include "../RTBEngineAPI.h"
#include "RHI/RenderTypes.h"

namespace RTBEngine {
    namespace Rendering {
        class Cubemap;
        class Shader;
        class Camera;
    }
}

//Website to create skyboxes: https://tools.wwwtyro.net/space-3d/index.html
namespace RTBEngine {
    namespace Rendering {

        class RTB_API Skybox {
        public:
            Skybox();
            ~Skybox();

            Skybox(const Skybox&) = delete;
            Skybox& operator=(const Skybox&) = delete;

            bool Initialize(Cubemap* cubemap, Shader* shader);

            void SetCubemap(Cubemap* cubemap);
            Cubemap* GetCubemap() const { return cubemap; }

            void Render(Camera* camera);

            void SetEnabled(bool enabled) { this->enabled = enabled; }
            bool IsEnabled() const { return enabled; }

        private:
            void CreateCubeMesh();
            void DeleteCubeMesh();

            Cubemap* cubemap = nullptr;
            Shader* shader = nullptr;
            bool enabled = true;

            RHI::GpuId VAO = RHI::kInvalidGpuId;
            RHI::GpuId VBO = RHI::kInvalidGpuId;
        };

    }
}
