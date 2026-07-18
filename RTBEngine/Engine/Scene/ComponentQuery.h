#pragma once

#include "../RTBEngineAPI.h"
#include "../Core/TypeId.h"
#include "Component.h"
#include <type_traits>
#include <vector>

namespace RTBEngine {
    namespace Scene {

        class Scene;

        namespace ComponentQueryDetail {
            template<typename T, typename = void>
            struct HasTypeId : std::false_type {};

            template<typename T>
            struct HasTypeId<T, std::void_t<decltype(T::TypeId())>> : std::true_type {};
        }

        class RTB_API ComponentQuery {
        public:
            static const std::vector<Component*>& GetComponentsByTypeName(const char* typeName);
            static Component* FindFirstComponentByTypeName(const char* typeName);

            static const std::vector<Component*>& GetComponentsByTypeId(std::uint32_t typeId);
            static Component* FindFirstComponentByTypeId(std::uint32_t typeId);

            template<typename T>
            static const std::vector<Component*>& GetComponents()
            {
                static_assert(ComponentQueryDetail::HasTypeId<T>::value,
                    "ComponentQuery::GetComponents<T> requires T::TypeId().");
                return GetComponentsByTypeId(T::TypeId());
            }

            template<typename T>
            static T* FindFirst()
            {
                static_assert(ComponentQueryDetail::HasTypeId<T>::value,
                    "ComponentQuery::FindFirst<T> requires T::TypeId().");

                for (Component* component : GetComponents<T>()) {
                    if (T* typed = dynamic_cast<T*>(component)) {
                        return typed;
                    }
                }

                return nullptr;
            }
        };

    }
}
