#include "Light.h"

namespace RTBEngine {
    namespace Rendering {

        Light::Light(LightType type)
            : type(type), color(1.0f, 1.0f, 1.0f), intensity(1.0f)
        {
        }

    }
}