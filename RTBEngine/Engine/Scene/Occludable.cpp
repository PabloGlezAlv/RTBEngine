#include "Occludable.h"
#include "GameObject.h"
#include "MeshRenderer.h"

namespace RTBEngine {
    namespace Scene {

        using ThisClass = Occludable;
        RTB_REGISTER_COMPONENT(Occludable)
            RTB_PROPERTY(occluderEnabled)
            RTB_PROPERTY(boundsPadding)
        RTB_END_REGISTER(Occludable)

        MeshRenderer* Occludable::GetMeshRenderer() const
        {
            return owner ? owner->GetComponent<MeshRenderer>() : nullptr;
        }

    }
}
