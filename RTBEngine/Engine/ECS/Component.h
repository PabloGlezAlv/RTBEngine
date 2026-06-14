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

        enum class ComponentTimeMode {
            Scaled,
            Unscaled
        };

        class RTB_API Component {
        public:
            Component();
            virtual ~Component();

            Component(const Component&) = delete;
            Component& operator=(const Component&) = delete;

            // Lifecycle hooks (orchestrated by SceneLifecycle — do not call manually).
            virtual void OnAwake() {}
            virtual void OnStart() {}
            virtual void OnUpdate(float deltaTime) {}
            virtual void OnFixedUpdate(float fixedDeltaTime) {}
            virtual void OnLateUpdate(float deltaTime) {}
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
            void SetTimeMode(ComponentTimeMode mode);
            ComponentTimeMode GetTimeMode() const { return timeMode; }

            bool HasAwakeBeenInvoked() const { return awakeInvoked; }
            bool HasStartBeenInvoked() const { return startInvoked; }
            void InvokeAwakeIfNeeded();
            void TryInvokeStart();

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
            ComponentTimeMode timeMode;
            bool awakeInvoked = false;
            bool startInvoked = false;
        };

    }
}
