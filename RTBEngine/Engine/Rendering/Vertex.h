#pragma once
#include "../Math/Math.h"

namespace RTBEngine {
    namespace Rendering {

        struct Vertex {
            Math::Vector3 position;
            Math::Vector3 normal;
            Math::Vector2 texCoords;
        };

    }
}