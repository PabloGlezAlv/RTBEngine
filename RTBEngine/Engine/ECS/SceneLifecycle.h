#pragma once

#include "../RTBEngineAPI.h"
#include <vector>

namespace RTBEngine {
    namespace ECS {

        class Component;
        class GameObject;
        class Scene;

        // Central orchestrator for GameObject/component initialization phases:
        // Deserialize (elsewhere) -> Wire (elsewhere) -> Awake -> Validate -> Start (per-component tick).
        class RTB_API SceneLifecycle {
        public:
            static void CollectHierarchyPreOrder(
                GameObject* root,
                std::vector<GameObject*>& outHierarchy,
                bool skipAlreadyCollected = false);

            static void CollectSceneHierarchyPreOrder(Scene* scene, std::vector<GameObject*>& outHierarchy);

            static void InvokeAwake(const std::vector<GameObject*>& hierarchy);
            static void InvokeValidate(const std::vector<GameObject*>& hierarchy);
            static void InvokeAwakeAndValidate(Component* component);

            static void BringHierarchyToLife(Scene* scene, GameObject* root);
            static void BringSceneToLife(Scene* scene);
        };

    }
}
