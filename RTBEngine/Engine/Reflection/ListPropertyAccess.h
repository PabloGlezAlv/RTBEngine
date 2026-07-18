#pragma once

#include "TypeInfo.h"
#include "../Scene/Component.h"
#include "../Scene/GameObject.h"
#include "../Animation/Animator.h"

#include <string>
#include <vector>

namespace RTBEngine {
    namespace Reflection {
        namespace ListPropertyAccess {

            // propertyPtr must come from PropertyInfo::GetMutableData (already includes offset).
            inline std::vector<std::string>* AsStringVector(void* propertyPtr)
            {
                return reinterpret_cast<std::vector<std::string>*>(propertyPtr);
            }

            inline std::vector<Scene::GameObject*>* AsGameObjectVector(void* propertyPtr)
            {
                return reinterpret_cast<std::vector<Scene::GameObject*>*>(propertyPtr);
            }

            inline std::vector<Scene::Component*>* AsComponentVector(void* propertyPtr)
            {
                return reinterpret_cast<std::vector<Scene::Component*>*>(propertyPtr);
            }

            inline std::vector<Animation::AnimationKeyClip>* AsAnimationKeyClipVector(void* propertyPtr)
            {
                return reinterpret_cast<std::vector<Animation::AnimationKeyClip>*>(propertyPtr);
            }

            inline std::vector<std::string>* GetStringVector(Scene::Component* component, const PropertyInfo& prop)
            {
                return AsStringVector(prop.GetMutableData(component));
            }

            inline std::vector<Scene::GameObject*>* GetGameObjectVector(Scene::Component* component, const PropertyInfo& prop)
            {
                return AsGameObjectVector(prop.GetMutableData(component));
            }

            inline std::vector<Scene::Component*>* GetComponentVector(Scene::Component* component, const PropertyInfo& prop)
            {
                return AsComponentVector(prop.GetMutableData(component));
            }

            inline std::vector<Animation::AnimationKeyClip>* GetAnimationKeyClipVector(
                Scene::Component* component, const PropertyInfo& prop)
            {
                return AsAnimationKeyClipVector(prop.GetMutableData(component));
            }

        }
    }
}
