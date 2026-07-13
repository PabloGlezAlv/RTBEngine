#include "SceneSaver.h"
#include <sstream>
#include <filesystem>

#include "ScenePropertySerializer.h"
#include "../Scene/Scene.h"
#include "../Scene/GameObject.h"
#include "../Scene/Component.h"
#include "../Scene/Transform.h"
#include "../Scene/Prefab.h"
#include "../Scene/PrefabRegistry.h"
#include "../Scene/PrefabOverrideDiff.h"
#include "../Scene/PrefabInstanceResolver.h"
#include "../Math/Vectors/Vector3.h"
#include "../Math/Quaternions/Quaternion.h"
#include "../Rendering/Cubemap.h"
#include "../Core/ResourceManager.h"
#include "../Physics/PhysicsLayerSettings.h"
#include "../RTBEngine.h"

namespace RTBEngine {
    namespace Scripting {

        bool SceneSaver::SaveScene(const ECS::Scene* scene, const std::string& filePath) {
            if (!scene) {
                RTB_ERROR("SceneSaver: Cannot save null scene");
                return false;
            }

            const std::string resolvedFilePath = Core::ResourceManager::GetInstance().ResolvePathForRead(filePath);
            const std::filesystem::path outputPath(resolvedFilePath);
            std::error_code ec;
            const std::filesystem::path parentPath = outputPath.parent_path();
            if (!parentPath.empty()) {
                std::filesystem::create_directories(parentPath, ec);
                if (ec) {
                    RTB_ERROR("SceneSaver: Failed to create parent directory for: " + filePath);
                    return false;
                }
            }

            std::ofstream file(outputPath);
            if (!file.is_open()) {
                RTB_ERROR("SceneSaver: Failed to open file for writing: " + filePath);
                return false;
            }

            try {
                file << "function CreateScene()\n";
                file << "    return {\n";

                WriteSceneHeader(file, scene);
                WriteGameObjects(file, scene);

                file << "    }\n";
                file << "end\n";

                file.close();
                RTB_INFO("SceneSaver: Successfully saved scene to: " + filePath);
                return true;
            }
            catch (const std::exception& e) {
                RTB_ERROR("SceneSaver: Exception during save: " + std::string(e.what()));
                return false;
            }
        }

        void SceneSaver::WriteSceneHeader(std::ofstream& file, const ECS::Scene* scene) {
            file << "        name = \"" << scene->GetName() << "\",\n";
            file << "        skyboxEnabled = " << ScenePropertySerializer::FormatBool(scene->IsSkyboxEnabled()) << ",\n";

            Rendering::Cubemap* cubemap = scene->GetSkyboxCubemap();
            if (cubemap) {
                std::string path = Core::ResourceManager::GetInstance().GetCubemapPath(cubemap);
                if (!path.empty()) {
                    file << "        skybox = " << ScenePropertySerializer::FormatString(
                        ScenePropertySerializer::NormalizePath(path)) << ",\n";
                }
            }
        }

        void SceneSaver::WriteGameObjects(std::ofstream& file, const ECS::Scene* scene) {
            const auto& gameObjects = scene->GetGameObjects();

            file << "        gameObjects = {\n";

            for (const auto& go : gameObjects) {
                if (go->GetParent() == nullptr) {
                    WriteGameObject(file, go.get(), 3);
                }
            }

            file << "        }\n";
        }

        namespace {
            const ECS::Prefab* FindDirectChildPrefab(const ECS::Prefab* prefab, const std::string& childName)
            {
                if (!prefab) {
                    return nullptr;
                }

                for (const auto& childPrefab : prefab->GetChildPrefabs()) {
                    if (childPrefab && childPrefab->GetName() == childName) {
                        return childPrefab.get();
                    }
                }
                return nullptr;
            }
        }

        void SceneSaver::WriteGameObject(std::ofstream& file, const ECS::GameObject* go, int indent,
            const ECS::Prefab* baselinePrefab)
        {
            if (go->IsTransient()) {
                return;
            }

            if (baselinePrefab && !ECS::PrefabOverrideDiff::ShouldPersistPrefabChild(go, baselinePrefab)) {
                return;
            }

            std::string ind = ScenePropertySerializer::Indent(indent);

            file << ind << "{\n";
            file << ind << "    name = \"" << go->GetName() << "\",\n";
            file << ind << "    uuid = \"" << go->GetUUID() << "\",\n";

            const bool hasRegisteredPrefab = go->IsPrefabInstance() &&
                ECS::PrefabRegistry::GetInstance().Has(go->GetPrefabName());

            const bool isPrefabChildOverride = baselinePrefab &&
                !ECS::PrefabOverrideDiff::IsSceneOnlyChild(go, baselinePrefab) &&
                !hasRegisteredPrefab;

            if (!isPrefabChildOverride) {
                if (!go->IsActive()) {
                    file << ind << "    active = "
                        << ScenePropertySerializer::FormatBool(go->IsActive()) << ",\n";
                }

                if (go->GetCollisionLayer() != 0) {
                    file << ind << "    collisionLayer = "
                        << ScenePropertySerializer::FormatString(
                            Physics::PhysicsLayerSettings::Get().GetLayerName(go->GetCollisionLayer()))
                        << ",\n";
                }
            }

            const ECS::Prefab* instancePrefab = nullptr;
            if (hasRegisteredPrefab) {
                instancePrefab = ECS::PrefabRegistry::GetInstance().Get(go->GetPrefabName());
                WritePrefabInstance(file, go, indent);
            }
            else if (baselinePrefab &&
                !ECS::PrefabOverrideDiff::IsSceneOnlyChild(go, baselinePrefab))
            {
                WritePrefabNodePersistence(file, go, indent, baselinePrefab);
            }
            else
            {
                WriteTransform(file, go, indent + 1);
                WriteComponents(file, go, indent + 1);
            }

            const ECS::Prefab* childLookupPrefab = instancePrefab ? instancePrefab : baselinePrefab;
            std::vector<const ECS::GameObject*> persistableChildren;
            const auto& children = go->GetChildren();
            for (const ECS::GameObject* child : children) {
                if (!child) {
                    continue;
                }

                if (childLookupPrefab &&
                    ECS::PrefabOverrideDiff::IsSceneOnlyChild(child, childLookupPrefab))
                {
                    persistableChildren.push_back(child);
                    continue;
                }

                const ECS::Prefab* childBaseline = childLookupPrefab
                    ? FindDirectChildPrefab(childLookupPrefab, child->GetName())
                    : nullptr;
                if (childBaseline &&
                    ECS::PrefabOverrideDiff::ShouldPersistPrefabChild(child, childBaseline))
                {
                    persistableChildren.push_back(child);
                    continue;
                }

                if (!childLookupPrefab) {
                    persistableChildren.push_back(child);
                }
            }

            if (!persistableChildren.empty()) {
                file << ind << "    children = {\n";
                for (const ECS::GameObject* child : persistableChildren) {
                    const ECS::Prefab* childBaseline = nullptr;
                    if (instancePrefab) {
                        childBaseline = FindDirectChildPrefab(instancePrefab, child->GetName());
                    } else if (baselinePrefab) {
                        childBaseline = FindDirectChildPrefab(baselinePrefab, child->GetName());
                    }
                    WriteGameObject(file, child, indent + 2, childBaseline);
                }
                file << ind << "    }\n";
            }

            file << ind << "},\n";
        }


        void SceneSaver::WriteTransform(std::ofstream& file, const ECS::GameObject* go, int indent) {
            if (go->IsAnimatorBone()) {
                return;
            }

            std::string ind = ScenePropertySerializer::Indent(indent);
            const auto& transform = go->GetTransform();

            Math::Vector3 pos = transform.GetPosition();
            Math::Vector3 rot = transform.GetRotation().ToEulerAngles();
            Math::Vector3 scale = transform.GetScale();

            if (pos.x != 0.0f || pos.y != 0.0f || pos.z != 0.0f)
                file << ind << "position = " << ScenePropertySerializer::FormatVector3(pos) << ",\n";

            if (rot.x != 0.0f || rot.y != 0.0f || rot.z != 0.0f)
                file << ind << "rotation = " << ScenePropertySerializer::FormatQuaternion(transform.GetRotation()) << ",\n";

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f)
                file << ind << "scale = " << ScenePropertySerializer::FormatVector3(scale) << ",\n";
        }

        void SceneSaver::WriteComponents(std::ofstream& file, const ECS::GameObject* go, int indent) {
            std::string ind = ScenePropertySerializer::Indent(indent);
            const auto& components = go->GetComponents();

            if (components.empty())
                return;

            file << ind << "components = {\n";

            for (const auto& comp : components)
                ScenePropertySerializer::WriteComponent(file, comp.get(), indent + 1);

            bool hasChildren = !go->GetChildren().empty();
            file << ind << (hasChildren ? "},\n" : "}\n");
        }

        void SceneSaver::WritePrefabInstance(std::ofstream& file, const ECS::GameObject* go, int indent) {
            std::string ind = ScenePropertySerializer::Indent(indent);

            file << ind << "    prefab = \"" << go->GetPrefabName() << "\",\n";

            WriteTransform(file, go, indent + 1);

            file << ind << "    overrides = {\n";
            WritePrefabOverrides(file, go, indent + 2);
            file << ind << "    },\n";
        }

        void SceneSaver::WritePrefabOverrides(std::ofstream& file, const ECS::GameObject* go, int indent) {
            std::string ind = ScenePropertySerializer::Indent(indent);

            const ECS::PrefabInstanceContext context = ECS::PrefabInstanceResolver::Resolve(
                const_cast<ECS::GameObject*>(go));
            if (!context.IsValid()) {
                return;
            }

            const auto& components = go->GetComponents();

            file << ind << "components = {\n";

            for (const auto& comp : components)
            {
                const std::string typeName = comp->GetTypeName();
                const ECS::ComponentSnapshot* snap = ECS::PrefabOverrideDiff::FindBaselineSnapshot(
                    context.baselineNode,
                    typeName.c_str());

                if (!snap)
                {
                    ScenePropertySerializer::WriteComponent(file, comp.get(), indent + 1);
                    continue;
                }

                const std::vector<const Reflection::PropertyInfo*> overrideProps =
                    ECS::PrefabOverrideDiff::GetOverriddenProperties(comp.get(), snap);

                if (overrideProps.empty()) continue;

                file << ind << "    { type = \"" << typeName << "\",\n";

                for (const Reflection::PropertyInfo* prop : overrideProps)
                {
                    ScenePropertySerializer::WriteProperty(file, comp.get(), *prop, indent + 2);
                    file << ",\n";
                }

                file << ind << "    },\n";
            }

            file << ind << "},\n";
        }

        void SceneSaver::WritePrefabNodePersistence(std::ofstream& file, const ECS::GameObject* go, int indent,
            const ECS::Prefab* baselineNode)
        {
            if (!go || !baselineNode) {
                return;
            }

            std::string ind = ScenePropertySerializer::Indent(indent);

            if (ECS::PrefabOverrideDiff::IsTransformOverridden(go, baselineNode)) {
                WriteTransform(file, go, indent + 1);
            }

            if (!go->IsActive() && ECS::PrefabOverrideDiff::IsActiveOverridden(go)) {
                file << ind << "    active = "
                    << ScenePropertySerializer::FormatBool(go->IsActive()) << ",\n";
            }

            if (ECS::PrefabOverrideDiff::IsCollisionLayerOverridden(go, baselineNode)) {
                file << ind << "    collisionLayer = "
                    << ScenePropertySerializer::FormatString(
                        Physics::PhysicsLayerSettings::Get().GetLayerName(go->GetCollisionLayer()))
                    << ",\n";
            }

            file << ind << "    overrides = {\n";
            WritePrefabOverrides(file, go, indent + 2);
            file << ind << "    },\n";
        }
    }
}
