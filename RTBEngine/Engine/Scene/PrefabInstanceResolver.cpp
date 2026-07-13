#include "PrefabInstanceResolver.h"
#include "GameObject.h"
#include "Prefab.h"
#include "PrefabRegistry.h"

namespace RTBEngine {
    namespace ECS {

        const Prefab* PrefabInstanceResolver::FindBaselineChild(
            const Prefab* rootBaseline,
            const std::vector<std::string>& nodePath)
        {
            const Prefab* current = rootBaseline;
            if (!current) {
                return nullptr;
            }

            for (const std::string& segment : nodePath) {
                const Prefab* next = nullptr;
                for (const auto& child : current->GetChildPrefabs()) {
                    if (child && child->GetName() == segment) {
                        next = child.get();
                        break;
                    }
                }
                if (!next) {
                    return nullptr;
                }
                current = next;
            }

            return current;
        }

        PrefabInstanceContext PrefabInstanceResolver::Resolve(GameObject* gameObject)
        {
            PrefabInstanceContext context;
            context.target = gameObject;
            if (!gameObject || gameObject->IsTransient()) {
                return context;
            }

            GameObject* walk = gameObject;
            GameObject* instanceRoot = nullptr;
            while (walk) {
                if (!walk->GetPrefabName().empty() &&
                    PrefabRegistry::GetInstance().Has(walk->GetPrefabName())) {
                    instanceRoot = walk;
                    break;
                }
                walk = walk->GetParent();
            }

            if (!instanceRoot) {
                return context;
            }

            context.instanceRoot = instanceRoot;
            context.assetName = instanceRoot->GetPrefabName();
            context.assetFilePath = PrefabRegistry::GetInstance().GetFilePath(context.assetName);

            const Prefab* assetRoot = PrefabRegistry::GetInstance().Get(context.assetName);
            if (!assetRoot) {
                context.instanceRoot = nullptr;
                return context;
            }

            std::vector<std::string> nodePath;
            walk = gameObject;
            while (walk && walk != instanceRoot) {
                nodePath.insert(nodePath.begin(), walk->GetName());
                walk = walk->GetParent();
            }

            context.nodePath = nodePath;
            context.baselineNode = FindBaselineChild(assetRoot, nodePath);
            if (!context.baselineNode) {
                context.instanceRoot = nullptr;
            }

            return context;
        }

    }
}
