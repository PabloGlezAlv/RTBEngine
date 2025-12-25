#pragma once
#include <string>

namespace RTBEngine {
    namespace Physics {
        struct CollisionInfo;
    }

    namespace ECS {

        class GameObject;

        class Component {
        public:
            Component();
            virtual ~Component();

            Component(const Component&) = delete;
            Component& operator=(const Component&) = delete;

            //Loop methods
            virtual void OnAwake() {}
            virtual void OnStart() {}
            virtual void OnUpdate(float deltaTime) {}
            virtual void OnFixedUpdate(float fixedDeltaTime) {}
            virtual void OnDestroy() {}

            //Collision methods
            // Collision callbacks
            virtual void OnCollisionEnter(const Physics::CollisionInfo& collision) {}
            virtual void OnCollisionStay(const Physics::CollisionInfo& collision) {}
            virtual void OnCollisionExit(const Physics::CollisionInfo& collision) {}

            // Trigger callbacks
            virtual void OnTriggerEnter(const Physics::CollisionInfo& collision) {}
            virtual void OnTriggerStay(const Physics::CollisionInfo& collision) {}
            virtual void OnTriggerExit(const Physics::CollisionInfo& collision) {}

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