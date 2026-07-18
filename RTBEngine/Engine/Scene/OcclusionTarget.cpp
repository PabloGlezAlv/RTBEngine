#include "OcclusionTarget.h"
#include "GameObject.h"

namespace RTBEngine {
    namespace Scene {

        using ThisClass = OcclusionTarget;
        RTB_REGISTER_COMPONENT(OcclusionTarget)
            RTB_PROPERTY(targetEnabled)
            RTB_PROPERTY(focusOffset)
        RTB_END_REGISTER(OcclusionTarget)

        bool OcclusionTarget::IsActiveTarget() const
        {
            if (!IsEnabled() || !targetEnabled || !owner) {
                return false;
            }

            return owner->IsActiveInHierarchy();
        }

        Math::Vector3 OcclusionTarget::GetFocusPosition() const
        {
            if (!owner) {
                return Math::Vector3::Zero();
            }

            Math::Vector3 focusPosition = owner->GetWorldPosition();
            focusPosition.x += focusOffset.x;
            focusPosition.y += focusOffset.y;
            focusPosition.z += focusOffset.z;
            return focusPosition;
        }

    }
}
