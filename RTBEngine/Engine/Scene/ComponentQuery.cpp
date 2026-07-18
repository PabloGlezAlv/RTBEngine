#include "ComponentQuery.h"

#include "Scene.h"
#include "SceneManager.h"

namespace RTBEngine {
    namespace Scene {

        namespace {

            static const std::vector<Component*> kEmptyComponents;

        }

        const std::vector<Component*>& ComponentQuery::GetComponentsByTypeName(const char* typeName)
        {
            if (!typeName || typeName[0] == '\0') {
                return kEmptyComponents;
            }

            Scene* scene = SceneManager::GetInstance().GetActiveScene();
            if (!scene) {
                return kEmptyComponents;
            }

            return scene->GetCachedComponentsByTypeName(typeName);
        }

        Component* ComponentQuery::FindFirstComponentByTypeName(const char* typeName)
        {
            const std::vector<Component*>& components = GetComponentsByTypeName(typeName);
            return components.empty() ? nullptr : components.front();
        }

        const std::vector<Component*>& ComponentQuery::GetComponentsByTypeId(std::uint32_t typeId)
        {
            if (typeId == 0) {
                return kEmptyComponents;
            }

            Scene* scene = SceneManager::GetInstance().GetActiveScene();
            if (!scene) {
                return kEmptyComponents;
            }

            return scene->GetCachedComponentsByTypeId(typeId);
        }

        Component* ComponentQuery::FindFirstComponentByTypeId(std::uint32_t typeId)
        {
            const std::vector<Component*>& components = GetComponentsByTypeId(typeId);
            return components.empty() ? nullptr : components.front();
        }

    }
}
