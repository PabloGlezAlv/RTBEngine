#pragma once

#include "../RTBEngineAPI.h"
#include "StaticFlags.h"

namespace RTBEngine {
    namespace Scene {

        class MeshRenderer;

        RTB_API bool RendererContributesGI(MeshRenderer* renderer);
        RTB_API bool RendererUsesStaticBatching(const MeshRenderer* renderer);
        RTB_API bool RendererIsStaticOccluder(MeshRenderer* renderer);

    }
}
