#pragma once
#include <string>

namespace RTBEngine {
    namespace ECS {

        class GameObject;

        class Component {
        public:
            Component();
            virtual ~Component();

            Component(const Component&) = delete;
            Component& operator=(const Component&) = delete;

            virtual void OnAwake() {}
            virtual void OnStart() {}
            virtual void OnUpdate(float deltaTime) {}
            virtual void OnDestroy() {}

            void SetOwner(GameObject* owner);
            GameObject* GetOwner() const { return owner; }

            void SetEnabled(bool enabled);
            bool IsEnabled() const { return isEnabled; }

            virtual const char* GetTypeName() const = 0;

        protected:
            GameObject* owner;
            bool isEnabled;
        };

    }
}