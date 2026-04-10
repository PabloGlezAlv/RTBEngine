#pragma once
#include "../RTBEngineAPI.h"
#include <string>

namespace RTBEngine {
    namespace Reflection {
        class TypeInfo;
    }

    namespace Physics {
        struct CollisionInfo;
    }

    namespace ECS {

        class GameObject;

        class RTB_API Component {
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
            
            // Editor methods
            virtual void OnValidate() {}

            // Called after the owning GameObject changes parent in the hierarchy.
            // Default implementation is a no-op; components override when they need
            // to react to hierarchy changes without coupling ECS to higher layers.
            virtual void OnParentChanged(GameObject* oldParent, GameObject* newParent) {}

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
            void SetUpdateTickEnabled(bool enabled);
            bool IsUpdateTickEnabled() const { return updateTickEnabled; }

            virtual const char* GetTypeName() const = 0;

            // Multiple inheritance reflection fix
            virtual void* GetActualObject() { return this; }
            virtual const void* GetActualObject() const { return this; }

            // Returns type info for inspector. Components using RTB_COMPONENT override this.
            virtual const Reflection::TypeInfo* GetTypeInfo() const { return nullptr; }

        protected:
            GameObject* owner;
            bool isEnabled;
            bool updateTickEnabled;
        };

    }
}
