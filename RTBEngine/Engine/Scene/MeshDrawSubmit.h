#pragma once

#include "../RTBEngineAPI.h"
#include "../Math/Matrix/Matrix4.h"
#include "../Math/Vectors/Vector4.h"
#include <cstddef>

namespace RTBEngine {
    namespace Animation {
        class Animator;
    }
    namespace Rendering {
        class Mesh;
        class Shader;
    }

    namespace Scene {

        // Shared mesh draw submission used by MeshRenderer, Scene opaque batching, etc.
        RTB_API void SubmitSingleMeshDraw(
            Rendering::Mesh* mesh,
            Rendering::Shader* shader,
            const Math::Matrix4& modelMatrix,
            bool hasAnimation,
            Animation::Animator* animator);

        RTB_API void SubmitInstancedMeshDraw(
            Rendering::Mesh* mesh,
            Rendering::Shader* shader,
            const Math::Matrix4* instanceMatrices,
            std::size_t instanceCount,
            const Math::Vector4* instanceColors = nullptr,
            bool useInstanceColors = false);

    }
}
