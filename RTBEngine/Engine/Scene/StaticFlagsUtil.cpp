#include "StaticFlagsUtil.h"

#include "GameObject.h"
#include "MeshRenderer.h"
#include "../Animation/Animator.h"

namespace RTBEngine {
    namespace Scene {

        bool RendererContributesGI(MeshRenderer* renderer)
        {
            if (!renderer || !renderer->IsEnabled()) {
                return false;
            }

            const GameObject* owner = renderer->GetOwner();
            if (!owner || !owner->IsActiveInHierarchy()) {
                return false;
            }

            if (!owner->HasStaticFlag(StaticFlags::ContributeGI)) {
                return false;
            }

            Animation::Animator* animator = renderer->GetActiveAnimator();
            return !(animator && animator->ShouldSkinMesh());
        }

        bool RendererUsesStaticBatching(const MeshRenderer* renderer)
        {
            if (!renderer) {
                return false;
            }

            const GameObject* owner = renderer->GetOwner();
            return owner && owner->HasStaticFlag(StaticFlags::Batching);
        }

        bool RendererIsStaticOccluder(MeshRenderer* renderer)
        {
            if (!renderer || !renderer->IsEnabled()) {
                return false;
            }

            const GameObject* owner = renderer->GetOwner();
            return owner && owner->IsActiveInHierarchy() && owner->HasStaticFlag(StaticFlags::Occluder);
        }

    }
}
