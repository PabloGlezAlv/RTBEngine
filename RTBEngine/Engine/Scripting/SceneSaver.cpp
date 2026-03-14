#include "SceneSaver.h"
#include <sstream>

#include "ScenePropertySerializer.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/Component.h"
#include "../ECS/Transform.h"
#include "../ECS/Prefab.h"
#include "../ECS/PrefabRegistry.h"
#include "../Math/Vectors/Vector3.h"
#include "../Math/Quaternions/Quaternion.h"
#include "../Rendering/Cubemap.h"
#include "../Core/ResourceManager.h"
#include "../RTBEngine.h"

namespace RTBEngine {
    namespace Scripting {

        bool SceneSaver::SaveScene(const ECS::Scene* scene, const std::string& filePath) {
            if (!scene) {
                RTB_ERROR("SceneSaver: Cannot save null scene");
                return false;
            }

            std::ofstream file(filePath);
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

        void SceneSaver::WriteGameObject(std::ofstream& file, const ECS::GameObject* go, int indent) {
            std::string ind = ScenePropertySerializer::Indent(indent);

            file << ind << "{\n";
            file << ind << "    name = \"" << go->GetName() << "\",\n";
            file << ind << "    uuid = \"" << go->GetUUID() << "\",\n";

            if (go->IsPrefabInstance())
            {
                WritePrefabInstance(file, go, indent);
            }
            else
            {
                WriteTransform(file, go, indent + 1);
                WriteComponents(file, go, indent + 1);
            }

            const auto& children = go->GetChildren();
            if (!children.empty()) {
                file << ind << "    children = {\n";
                for (const auto* child : children) {
                    WriteGameObject(file, child, indent + 2);
                }
                file << ind << "    }\n";
            }

            file << ind << "},\n";
        }


        void SceneSaver::WriteTransform(std::ofstream& file, const ECS::GameObject* go, int indent) {
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

            const ECS::Prefab* prefab = ECS::PrefabRegistry::GetInstance().Get(go->GetPrefabName());
            if (!prefab)
                return;

            const auto& snapshots = prefab->GetSnapshots();
            const auto& components = go->GetComponents();

            file << ind << "components = {\n";

            for (const auto& comp : components)
            {
                const std::string typeName = comp->GetTypeName();

                const ECS::ComponentSnapshot* snap = nullptr;
                for (const auto& s : snapshots)
                {
                    if (s.typeName == typeName)
                    {
                        snap = &s;
                        break;
                    }
                }

                if (!snap)
                {
                    ScenePropertySerializer::WriteComponent(file, comp.get(), indent + 1);
                    continue;
                }

                const Reflection::TypeInfo* typeInfo = comp->GetTypeInfo();
                if (!typeInfo) continue;

                // First pass — detect which properties are overrides
                std::vector<const Reflection::PropertyInfo*> overrideProps;

                for (const Reflection::PropertyInfo* prop : typeInfo->GetSerializableProperties())
                {
                    size_t offset = prop->offset;
                    size_t size = prop->size;

                    if (prop->type == Reflection::PropertyType::String)
                    {
                        const std::string* liveStr = reinterpret_cast<const std::string*>(
                            reinterpret_cast<const char*>(comp.get()) + offset);

                        auto it = snap->stringData.find(offset);
                        if (it == snap->stringData.end() || it->second != *liveStr)
                            overrideProps.push_back(prop);
                        continue;
                    }

                    const uint8_t* raw = snap->rawData.data();
                    const uint8_t* rawEnd = raw + snap->rawData.size();
                    const uint8_t* snapBytes = nullptr;

                    while (raw < rawEnd)
                    {
                        size_t rawOffset = *reinterpret_cast<const size_t*>(raw); raw += sizeof(size_t);
                        size_t rawSize = *reinterpret_cast<const size_t*>(raw); raw += sizeof(size_t);

                        if (rawOffset == offset) { snapBytes = raw; break; }
                        raw += rawSize;
                    }

                    const char* liveBytes = reinterpret_cast<const char*>(comp.get()) + offset;

                    if (!snapBytes || std::memcmp(liveBytes, snapBytes, size) != 0)
                        overrideProps.push_back(prop);
                }

                if (overrideProps.empty()) continue;

                // Second pass — write only the overridden properties
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
    }
}
