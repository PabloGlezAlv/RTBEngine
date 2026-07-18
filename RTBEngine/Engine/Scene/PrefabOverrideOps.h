#pragma once

#include "../RTBEngineAPI.h"
#include "PrefabInstanceResolver.h"
#include "../Reflection/TypeInfo.h"

namespace RTBEngine {
    namespace Scene {

        class GameObject;
        class Component;
        class Scene;

        class RTB_API PrefabOverrideOps {
        public:
            // Revert restores a single overridden field from the PrefabRegistry baseline.
            static bool RevertProperty(
                GameObject* gameObject,
                Component* component,
                const Reflection::PropertyInfo* property);

            // Apply writes one overridden property from the live instance into the .prefab asset.
            // Intended workflow: one Apply call per changed field (editor: right-click on the property).
            static bool ApplyProperty(
                GameObject* gameObject,
                Component* component,
                const Reflection::PropertyInfo* property);

            static bool RevertTransform(GameObject* gameObject);
            // ApplyTransform updates position/rotation/scale on the asset when the transform differs.
            static bool ApplyTransform(GameObject* gameObject);

            static bool RevertAddedComponent(GameObject* gameObject, Component* component);
            static bool ApplyAddedComponent(GameObject* gameObject, Component* component);

            static bool RevertComponent(GameObject* gameObject, const char* typeName);
            static bool ApplyComponent(GameObject* gameObject, const char* typeName);

            static bool RevertAll(GameObject* gameObject, Scene* scene, GameObject** outReplacementRoot = nullptr);
            static bool ApplyAll(GameObject* gameObject);

            static bool IsPropertyOverridden(
                GameObject* gameObject,
                Component* component,
                const Reflection::PropertyInfo* property);
        };

    }
}
