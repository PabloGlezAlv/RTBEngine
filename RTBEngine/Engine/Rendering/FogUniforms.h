#pragma once

#include "../RTBEngineAPI.h"

namespace RTBEngine {
    namespace Rendering {

        class Shader;
        class Camera;

        // Uploads distance/height + volumetric fog params from LightingProjectSettings
        // into the currently bound shader (OpenGL loose uniforms / Vulkan PerDraw).
        class RTB_API FogUniforms {
        public:
            static void Apply(Shader* shader);
            static void ApplyCameraPlanes(Shader* shader, const Camera* camera);
        };

    }
}
