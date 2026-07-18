#include "MeshDrawSubmit.h"
#include "MeshRenderer.h"

#include "../Animation/Animator.h"
#include "../Rendering/Mesh.h"
#include "../Rendering/Shader.h"

namespace RTBEngine {
    namespace Scene {

        void SubmitSingleMeshDraw(
            Rendering::Mesh* mesh,
            Rendering::Shader* shader,
            const Math::Matrix4& modelMatrix,
            bool hasAnimation,
            Animation::Animator* animator)
        {
            if (!mesh || !shader) {
                return;
            }

            shader->SetBool("uUseInstancing", false);
            shader->SetBool("uUseInstanceColor", false);
            shader->SetMatrix4("uModel", modelMatrix);

            if (hasAnimation && animator) {
                shader->SetBool("uHasAnimation", true);
                animator->BindBoneMatrices();
            }
            else {
                shader->SetBool("uHasAnimation", false);
            }

            mesh->Draw();
            MeshRenderer::AddInstancedDrawStats(mesh->GetIndexCount(), 1);
        }

        void SubmitInstancedMeshDraw(
            Rendering::Mesh* mesh,
            Rendering::Shader* shader,
            const Math::Matrix4* instanceMatrices,
            std::size_t instanceCount,
            const Math::Vector4* instanceColors,
            bool useInstanceColors)
        {
            if (!mesh || !shader || !instanceMatrices || instanceCount == 0) {
                return;
            }

            shader->SetBool("uUseInstancing", true);
            shader->SetBool("uHasAnimation", false);
            shader->SetBool("uUseInstanceColor", useInstanceColors);
            if (useInstanceColors) {
                shader->SetVector4("uColor", Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            }

            mesh->UploadInstanceData(instanceMatrices, instanceCount);
            if (useInstanceColors && instanceColors) {
                mesh->UploadInstanceColors(instanceColors, instanceCount);
            }
            mesh->DrawInstanced(static_cast<int>(instanceCount));

            shader->SetBool("uUseInstancing", false);
            shader->SetBool("uUseInstanceColor", false);

            MeshRenderer::AddInstancedDrawStats(
                mesh->GetIndexCount(),
                static_cast<uint32_t>(instanceCount));
        }

    }
}
