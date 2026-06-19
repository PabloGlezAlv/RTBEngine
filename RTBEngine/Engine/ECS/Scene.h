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

    namespace ECS {

        class CameraComponent;
        class MeshRenderer;
        class TrailRenderer;
        class ParticleSystem;
        class RigidBodyComponent;

        #pragma warning(push)
        #pragma warning(disable: 4251)
        class RTB_API Scene {
        public:
            Scene(const std::string& name = "Untitled Scene");
            ~Scene();

            Scene(const Scene&) = delete;
            Scene& operator=(const Scene&) = delete;

            void AddGameObject(GameObject* gameObject, bool queueLifecycle = true);
            void RemoveGameObject(GameObject* gameObject);
            void BringGameObjectToLife(GameObject* root);
            bool IsLifecycleComplete() const { return lifecycleComplete; }
            void SetLifecycleComplete(bool complete) { lifecycleComplete = complete; }
            GameObject* FindGameObject(const std::string& name);
            GameObject* FindGameObjectByUUID(const std::string& uuid);

            void Update(float deltaTime);
            void FixedUpdate(float fixedDeltaTime);
            void LateUpdate(float deltaTime);
            void Render(Rendering::Camera* camera);
            void RenderTransparentEffects(Rendering::Camera* camera);

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

            const std::vector<MeshRenderer*>& GetCachedMeshRenderers() const;
            const std::vector<LightComponent*>& GetCachedLightComponents() const;
            const std::vector<TrailRenderer*>& GetCachedTrailRenderers() const;
            const std::vector<ParticleSystem*>& GetCachedParticleSystems() const;
            const std::vector<RTBEngine::UI::Canvas*>& GetCachedCanvases() const;
            const std::vector<RigidBodyComponent*>& GetCachedRigidBodies() const;

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
            void AssignGameObjectOwnership(GameObject* gameObject);
            void ClearGameObjectOwnership(GameObject* gameObject);
            void RegisterGameObjectUuid(GameObject* gameObject);
            void UnregisterGameObjectUuid(GameObject* gameObject);
            void RegisterGameObjectHierarchy(GameObject* root);
            void UnregisterGameObjectHierarchy(GameObject* root);

            friend class GameObject;

            std::string name;
            std::vector<std::unique_ptr<GameObject>> gameObjects;
            std::unordered_map<std::string, GameObject*> gameObjectsByUuid;

            std::vector<std::unique_ptr<GameObject>> pendingAdds;
            std::vector<GameObject*> pendingRemoves;
            std::vector<GameObject*> pendingLifecycleRoots;
            int iterationDepth = 0;

            bool lifecycleComplete = false;

            // Skybox settings
            Rendering::Cubemap* skyboxCubemap = nullptr;
            bool skyboxEnabled = true;

            bool pendingRenderLog = false;

            mutable bool componentCachesDirty = true;
            mutable std::vector<MeshRenderer*> cachedMeshRenderers;
            mutable std::vector<LightComponent*> cachedLightComponents;
            mutable std::vector<TrailRenderer*> cachedTrailRenderers;
            mutable std::vector<ParticleSystem*> cachedParticleSystems;
            mutable std::vector<RTBEngine::UI::Canvas*> cachedCanvases;
            mutable std::vector<RigidBodyComponent*> cachedRigidBodies;
        };
        #pragma warning(pop)

    }
}
