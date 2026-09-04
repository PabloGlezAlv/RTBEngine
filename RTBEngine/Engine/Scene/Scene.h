#pragma once
#include "../RTBEngineAPI.h"
#include "GameObject.h"
#include "../Rendering/Camera.h"
#include "LightComponent.h"
#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include <unordered_map>

namespace RTBEngine {
    namespace Rendering {
        class Cubemap;
    }
}


namespace RTBEngine {
    namespace UI {
        class Canvas;
    }

    namespace Scene {

        class CameraComponent;
        class MeshRenderer;
        class TrailRenderer;
        class ParticleSystem;
        class AnimatedBillboard;
        class RigidBodyComponent;
        class Occludable;
        class OcclusionTarget;
        class Component;

        #pragma warning(push)
        #pragma warning(disable: 4251)
        class RTB_API Scene {
        public:
            Scene(const std::string& name = "Untitled Scene");
            ~Scene();

            Scene(const Scene&) = delete;
            Scene& operator=(const Scene&) = delete;

            class RTB_API DispatchScope {
            public:
                explicit DispatchScope(Scene* scene);
                ~DispatchScope();

                DispatchScope(const DispatchScope&) = delete;
                DispatchScope& operator=(const DispatchScope&) = delete;

            private:
                Scene* scene;
            };

            bool IsDispatching() const { return dispatchDepth > 0; }

            void AddGameObject(GameObject* gameObject, bool queueLifecycle = true);
            void RemoveGameObject(GameObject* gameObject);
            void BringGameObjectToLife(GameObject* root);
            bool IsLifecycleComplete() const { return lifecycleComplete; }
            void SetLifecycleComplete(bool complete) { lifecycleComplete = complete; }
            GameObject* FindGameObject(const std::string& name);
            GameObject* FindGameObjectByUUID(const std::string& uuid);

            void Update();
            void FixedUpdate(float fixedDeltaTime);
            void LateUpdate();
            void Render(Rendering::Camera* camera);
            void RenderTransparentEffects(Rendering::Camera* camera);
            void TickEditModeSimulate(float deltaTime);

            // Skybox management (per-scene override)
            void SetSkyboxCubemap(Rendering::Cubemap* cubemap);
            Rendering::Cubemap* GetSkyboxCubemap() const { return skyboxCubemap; }
            void SetSkyboxEnabled(bool enabled) { skyboxEnabled = enabled; }
            bool IsSkyboxEnabled() const { return skyboxEnabled; }

            //Stats counters
            uint32_t GetActiveGameObjectCount() const;
            uint32_t GetActiveComponentCount() const;

            const std::string& GetName() const { return name; }
            void SetName(const std::string& newName) { name = newName; }
            const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return gameObjects; }

            void InvalidateComponentCaches();
            void InvalidateTickCache();

            const std::vector<MeshRenderer*>& GetCachedMeshRenderers() const;
            const std::vector<LightComponent*>& GetCachedLightComponents() const;
            const std::vector<TrailRenderer*>& GetCachedTrailRenderers() const;
            const std::vector<ParticleSystem*>& GetCachedParticleSystems() const;
            const std::vector<AnimatedBillboard*>& GetCachedAnimatedBillboards() const;
            const std::vector<RTBEngine::UI::Canvas*>& GetCachedCanvases() const;
            const std::vector<RigidBodyComponent*>& GetCachedRigidBodies() const;
            const std::vector<Occludable*>& GetCachedOccludables() const;
            const std::vector<OcclusionTarget*>& GetCachedOcclusionTargets() const;
            const std::vector<Component*>& GetCachedComponentsByTypeName(const char* typeName) const;
            const std::vector<Component*>& GetCachedComponentsByTypeId(std::uint32_t typeId) const;

            // Camera management
            void SetMainCamera(CameraComponent* camera);
            CameraComponent* GetMainCamera() const;
            Rendering::Camera* GetActiveCamera();

            // Editor Play: allow OnStart to fire again without reloading the scene.
            void PrepareForPlayMode();

        private:
            //Deferred command buffer
            void FlushPendingCommands();
            void QueueLifecycleInitialization(GameObject* root);
            void FlushPendingLifecycle();
            bool OwnsGameObject(GameObject* target) const;
            void EnsureComponentCaches() const;
            void RebuildComponentCaches() const;
            void EnsureTickCache() const;
            void RebuildTickCache() const;
            void AssignGameObjectOwnership(GameObject* gameObject);
            void ClearGameObjectOwnership(GameObject* gameObject);
            void RegisterGameObjectUuid(GameObject* gameObject);
            void UnregisterGameObjectUuid(GameObject* gameObject);
            void RegisterGameObjectHierarchy(GameObject* root);
            void UnregisterGameObjectHierarchy(GameObject* root);
            void TrackPendingComponentRemovals(GameObject* owner);
            void FlushPendingComponentRemovals();
            void TrackPendingStarts(GameObject* owner);
            void DrainPendingStarts();
            bool IsTickableNow(Component* component) const;

            friend class GameObject;

            std::string name;
            std::vector<std::unique_ptr<GameObject>> gameObjects;
            std::unordered_map<std::string, GameObject*> gameObjectsByUuid;

            std::vector<std::unique_ptr<GameObject>> pendingAdds;
            std::vector<GameObject*> pendingRemoves;
            std::vector<GameObject*> pendingLifecycleRoots;
            std::vector<GameObject*> gameObjectsWithPendingComponentRemovals;
            std::vector<GameObject*> gameObjectsWithPendingStarts;
            int dispatchDepth = 0;
            bool flushingPendingCommands = false;

            bool lifecycleComplete = false;

            // Skybox settings
            Rendering::Cubemap* skyboxCubemap = nullptr;
            bool skyboxEnabled = true;

            bool pendingRenderLog = false;

            mutable bool componentCachesDirty = true;
            mutable bool tickCacheDirty = true;
            mutable std::vector<MeshRenderer*> cachedMeshRenderers;
            mutable std::vector<LightComponent*> cachedLightComponents;
            mutable std::vector<TrailRenderer*> cachedTrailRenderers;
            mutable std::vector<ParticleSystem*> cachedParticleSystems;
            mutable std::vector<AnimatedBillboard*> cachedAnimatedBillboards;
            mutable std::vector<RTBEngine::UI::Canvas*> cachedCanvases;
            mutable std::vector<RigidBodyComponent*> cachedRigidBodies;
            mutable std::vector<Occludable*> cachedOccludables;
            mutable std::vector<OcclusionTarget*> cachedOcclusionTargets;
            mutable std::vector<Component*> cachedTickComponents;
            mutable std::vector<Component*> cachedTransparentRenderComponents;
            mutable std::vector<Component*> cachedEditModeSimulateComponents;
            mutable std::unordered_map<std::string, std::vector<Component*>> cachedComponentsByTypeName;
            mutable std::unordered_map<std::uint32_t, std::vector<Component*>> cachedComponentsByTypeId;
        };
        #pragma warning(pop)

    }
}
