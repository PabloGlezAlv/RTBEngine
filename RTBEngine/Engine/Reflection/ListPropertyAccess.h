#pragma once

#include "TypeInfo.h"
#include "../Scene/Component.h"
#include "../Scene/GameObject.h"

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

            inline std::vector<ECS::GameObject*>* AsGameObjectVector(void* propertyPtr)
            {
                return reinterpret_cast<std::vector<ECS::GameObject*>*>(propertyPtr);
            }

            inline std::vector<ECS::Component*>* AsComponentVector(void* propertyPtr)
            {
                return reinterpret_cast<std::vector<ECS::Component*>*>(propertyPtr);
            }

            inline std::vector<std::string>* GetStringVector(ECS::Component* component, const PropertyInfo& prop)
            {
                return AsStringVector(prop.GetMutableData(component));
            }

            inline std::vector<ECS::GameObject*>* GetGameObjectVector(ECS::Component* component, const PropertyInfo& prop)
            {
                return AsGameObjectVector(prop.GetMutableData(component));
            }

            inline std::vector<ECS::Component*>* GetComponentVector(ECS::Component* component, const PropertyInfo& prop)
            {
                return AsComponentVector(prop.GetMutableData(component));
            }

        }
    }
}
