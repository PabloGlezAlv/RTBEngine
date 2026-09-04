#pragma once
#include "../RTBEngineAPI.h"
#include "../Scripting/LatentActions.h"
#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>

namespace RTBEngine {
    namespace Reflection {
        class TypeInfo;
    }

    namespace Physics {
        struct CollisionInfo;
    }

    namespace Rendering {
        class Camera;
    }

    namespace Scene {

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
            virtual void OnEnable() {}
            virtual void OnStart() {}
            virtual void OnUpdate(float deltaTime) {}
            virtual void OnFixedUpdate(float fixedDeltaTime) {}
            virtual void OnLateUpdate(float deltaTime) {}
            virtual void OnDisable() {}
            virtual void OnDestroy() {}
            
            // Editor methods
            virtual void OnValidate() {}

            // Called after the owning GameObject changes parent in the hierarchy.
            // Default implementation is a no-op; components override when they need
            // to react to hierarchy changes without coupling the scene layer to higher layers.
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

            // Optional transparent/custom draw path for gameplay components (GameScripts).
            // Engine stays agnostic: Scene only calls these when WantsTransparentRender() is true.
            virtual bool WantsTransparentRender() const { return false; }
            virtual void OnTransparentRender(Rendering::Camera* camera) { (void)camera; }

            // Optional edit-mode simulation (Scene View) without play mode.
            virtual bool WantsEditModeSimulate() const { return false; }
            virtual void OnEditModeSimulate(float deltaTime) { (void)deltaTime; }

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
            void ResetStartInvocation();
            void InvokeAwakeIfNeeded();
            void TryInvokeStart();
            void SyncEnabledState();
            void NotifyDisabled();

            virtual const char* GetTypeName() const = 0;
            virtual std::size_t FillTypeIds(std::uint32_t* out, std::size_t capacity) const;

            // Multiple inheritance reflection fix
            virtual void* GetActualObject() { return this; }
            virtual const void* GetActualObject() const { return this; }

            // Returns type info for inspector. Components using RTB_COMPONENT override this.
            virtual const Reflection::TypeInfo* GetTypeInfo() const { return nullptr; }

            // Latent actions (engine Scheduler — Invoke, sequences, repeating). Cancelled automatically
            // when this component is destroyed. Respects ComponentTimeMode (Scaled / Unscaled).
            Scripting::LatentActionHandle StartSequence(Scripting::LatentSequence sequence);
            Scripting::LatentActionHandle Invoke(float delaySeconds, std::function<void()> callback);
            Scripting::LatentActionHandle InvokeRepeating(
                float initialDelaySeconds,
                float intervalSeconds,
                std::function<void()> callback);
            void CancelInvoke(Scripting::LatentActionHandle handle);
            void CancelAllInvokes();

        protected:
            GameObject* owner;
            bool isEnabled;
            bool updateTickEnabled;
            ComponentTimeMode timeMode;
            bool awakeInvoked = false;
            bool startInvoked = false;
            bool enabledInHierarchy = false;
        };

    }
}
