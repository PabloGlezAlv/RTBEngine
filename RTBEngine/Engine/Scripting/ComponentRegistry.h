#pragma once
#include "../RTBEngineAPI.h"
#include <string>
#include <unordered_map>
#include <functional>

namespace RTBEngine {
    namespace ECS {
        class Component;
    }
}

namespace RTBEngine {
    namespace Scripting {

        // C4251: STL members in DLL-exported class are safe here because
        // ComponentRegistry is a singleton — clients never copy or directly access them.
        #pragma warning(push)
        #pragma warning(disable: 4251)
        class RTB_API ComponentRegistry {
        public:
            static ComponentRegistry& GetInstance();

            // Register a component factory
            void RegisterComponent(const std::string& typeName,
                std::function<ECS::Component* ()> factory);

            // Create a component by type name
            ECS::Component* CreateComponent(const std::string& typeName);

            // Check if a component type is registered
            bool HasComponent(const std::string& typeName) const;

            void RegisterBuiltInComponents();

        private:
            ComponentRegistry() = default;
            ~ComponentRegistry() = default;

            // Delete copy/move
            ComponentRegistry(const ComponentRegistry&) = delete;
            ComponentRegistry& operator=(const ComponentRegistry&) = delete;

            std::unordered_map<std::string, std::function<ECS::Component* ()>> factories;
        };
        #pragma warning(pop)

    }
}
