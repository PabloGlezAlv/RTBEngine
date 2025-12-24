#include "ComponentRegistry.h"
#include <iostream>

namespace RTBEngine {
    namespace Scripting {

        ComponentRegistry& ComponentRegistry::GetInstance() {
            static ComponentRegistry instance;
            return instance;
        }

        void ComponentRegistry::RegisterComponent(const std::string& typeName,
            std::function<ECS::Component* ()> factory) {
            factories[typeName] = factory;
        }

        ECS::Component* ComponentRegistry::CreateComponent(const std::string& typeName) {
            auto it = factories.find(typeName);
            if (it != factories.end()) {
                return it->second();
            }

            std::cerr << "ComponentRegistry: Component type '" << typeName << "' not registered!" << std::endl;
            return nullptr;
        }

        bool ComponentRegistry::HasComponent(const std::string& typeName) const {
            return factories.find(typeName) != factories.end();
        }

    }
}
