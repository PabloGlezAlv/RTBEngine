#pragma once

#include "../RTBEngineAPI.h"
#include "../Reflection/TypeInfo.h"
#include "Prefab.h"
#include <vector>

namespace RTBEngine {
    namespace ECS {

        class GameObject;
        class Component;

        class RTB_API PrefabOverrideDiff {
        public:
            static const ComponentSnapshot* FindBaselineSnapshot(
                const Prefab* baselineNode,
                const char* typeName);

            static bool IsPropertyOverridden(
                const Component* component,
                const ComponentSnapshot* baselineSnapshot,
                const Reflection::PropertyInfo* property);

            static std::vector<const Reflection::PropertyInfo*> GetOverriddenProperties(
                const Component* component,
                const ComponentSnapshot* baselineSnapshot);

            static bool IsAddedComponent(
                const Component* component,
                const Prefab* baselineNode);

            static bool IsTransformOverridden(
                const GameObject* gameObject,
                const Prefab* baselineNode);

            static bool IsActiveOverridden(
                const GameObject* gameObject);

            static bool IsCollisionLayerOverridden(
                const GameObject* gameObject,
                const Prefab* baselineNode);

            static bool IsSceneOnlyChild(
                const GameObject* child,
                const Prefab* parentBaseline);

            static bool HasAnyComponentOverrides(
                const GameObject* gameObject,
                const Prefab* baselineNode);

            static bool HasAnyOverrides(
                const GameObject* gameObject,
                const Prefab* baselineNode);

            static bool ShouldPersistPrefabChild(
                const GameObject* gameObject,
                const Prefab* baselineNode);
        };

    }
}
