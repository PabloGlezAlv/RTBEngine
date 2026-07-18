#pragma once

#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Math/Vectors/Vector3.h"
#include "../Reflection/PropertyMacros.h"

namespace RTBEngine {
    namespace Scene {

        class RTB_API OcclusionTarget : public Component {
        public:
            OcclusionTarget() = default;
            ~OcclusionTarget() override = default;

            bool targetEnabled = true;
            Math::Vector3 focusOffset = Math::Vector3(0.0f, 1.2f, 0.0f);

            bool IsActiveTarget() const;
            Math::Vector3 GetFocusPosition() const;

            RTB_COMPONENT(OcclusionTarget)
        };

    }
}
