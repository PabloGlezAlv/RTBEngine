#include "Skybox.h"
#include "Cubemap.h"
#include "Shader.h"
#include "Camera.h"
#include "CameraUBO.h"
#include "RHI/RenderDevice.h"
#include "../Math/Matrix/Matrix4.h"

namespace RTBEngine {
    namespace Rendering {

        static const float skyboxVertices[] = {
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,

             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,

             -1.0f,  1.0f,  1.0f,
              1.0f,  1.0f,  1.0f,
              1.0f,  1.0f, -1.0f,
              1.0f,  1.0f, -1.0f,
             -1.0f,  1.0f, -1.0f,
             -1.0f,  1.0f,  1.0f,

             -1.0f, -1.0f, -1.0f,
              1.0f, -1.0f, -1.0f,
              1.0f, -1.0f,  1.0f,
              1.0f, -1.0f,  1.0f,
             -1.0f, -1.0f,  1.0f,
             -1.0f, -1.0f, -1.0f
        };

        namespace {
            RHI::IRenderDevice& Device()
            {
                return RHI::RenderDevice::Get();
            }
        }

        Skybox::Skybox()
            : cubemap(nullptr), shader(nullptr), enabled(true) {
        }

        Skybox::~Skybox() {
            DeleteCubeMesh();
        }

        bool Skybox::Initialize(Cubemap* cubemap, Shader* shader) {
            if (!cubemap || !shader) {
                return false;
            }

            this->cubemap = cubemap;
            this->shader = shader;

            CreateCubeMesh();
            return true;
        }

        void Skybox::SetCubemap(Cubemap* cubemap) {
            this->cubemap = cubemap;
        }

        void Skybox::CreateCubeMesh() {
            auto& device = Device();
            VAO = device.CreateVertexArray();
            VBO = device.CreateBuffer();

            device.BindVertexArray(VAO);
            device.SetArrayBufferData(VBO, skyboxVertices, sizeof(skyboxVertices), RHI::BufferUsage::Static);
            device.EnableVertexAttribFloat(0, 3, static_cast<int>(3 * sizeof(float)), 0);
            device.UnbindVertexArray();
        }

        void Skybox::DeleteCubeMesh() {
            if (!RHI::RenderDevice::HasDevice()) {
                VAO = RHI::kInvalidGpuId;
                VBO = RHI::kInvalidGpuId;
                return;
            }
            auto& device = Device();
            if (VAO != RHI::kInvalidGpuId) {
                device.DestroyVertexArray(VAO);
                VAO = RHI::kInvalidGpuId;
            }
            if (VBO != RHI::kInvalidGpuId) {
                device.DestroyBuffer(VBO);
                VBO = RHI::kInvalidGpuId;
            }
        }

        void Skybox::Render(Camera* camera) {
            if (!enabled || !cubemap || !shader || !camera) {
                return;
            }

            auto& device = Device();
            device.SetCullFace(false);
            device.SetDepthFunc(RHI::DepthFunc::LEqual);

            shader->Bind();
            CameraUBO::GetInstance().Bind();
            shader->SetInt("uSkybox", 0);

            cubemap->Bind(0);

            device.BindVertexArray(VAO);
            device.DrawArrays(RHI::PrimitiveTopology::Triangles, 0, 36);
            device.UnbindVertexArray();

            cubemap->Unbind();
            shader->Unbind();

            device.SetDepthFunc(RHI::DepthFunc::Less);
            device.SetCullFace(true);
        }

    }
}
