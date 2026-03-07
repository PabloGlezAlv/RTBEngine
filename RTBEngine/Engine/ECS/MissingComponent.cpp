#include "MissingComponent.h"

namespace RTBEngine {
    namespace ECS {

        MissingComponent::MissingComponent() {}

        MissingComponent::MissingComponent(const std::string& missingTypeName)
            : missingTypeName(missingTypeName) {}

        MissingComponent::~MissingComponent() {}

    }
}
