#include "MissingComponent.h"

namespace RTBEngine {
    namespace Scene {

        MissingComponent::MissingComponent() {}

        MissingComponent::MissingComponent(const std::string& missingTypeName)
            : missingTypeName(missingTypeName) {}

        MissingComponent::~MissingComponent() {}

    }
}
