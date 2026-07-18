#pragma once

#include "../../RTBEngineAPI.h"

namespace RTBEngine {
    namespace Scene {
        class GameObject;
    }

    namespace ECS {

        // Optional GameObject proxy for hybrid ECS simulation + scene visuals.
        struct RTB_API VisualLink {
            Scene::GameObject* visual = nullptr;
        };

    }
}
