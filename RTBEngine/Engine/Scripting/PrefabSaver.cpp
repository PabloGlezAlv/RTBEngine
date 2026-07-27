#include "PrefabSaver.h"
#include "../Scene/Prefab.h"
#include "../Scene/Component.h"
#include "ComponentRegistry.h"
#include "ScenePropertySerializer.h"
#include "../Math/Math.h"
#include "../Physics/PhysicsLayerSettings.h"
#include "../RTBEngine.h"
#include <fstream>
#include <filesystem>

namespace RTBEngine {
    namespace Scripting {

        //Internal helpers
        static std::string Indent(int depth)
        {
            return std::string(static_cast<size_t>(depth) * 4, ' ');
        }

        static void WriteTransform(std::ofstream& file, const Scene::Prefab& prefab, int depth)
        {
            std::string ind = Indent(depth);
            Math::Vector3 pos = prefab.GetPosition();
            Math::Vector3 rot = prefab.GetRotation().ToEulerAngles();
            Math::Vector3 scale = prefab.GetScale();

            if (pos.x != 0.0f || pos.y != 0.0f || pos.z != 0.0f)
                file << ind << "position = " << ScenePropertySerializer::FormatVector3(pos) << ",\n";

            if (rot.x != 0.0f || rot.y != 0.0f || rot.z != 0.0f)
                file << ind << "rotation = " << ScenePropertySerializer::FormatQuaternion(prefab.GetRotation()) << ",\n";

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f)
                file << ind << "scale = " << ScenePropertySerializer::FormatVector3(scale) << ",\n";
        }

        static void WriteNode(std::ofstream& file, const Scene::Prefab& prefab, int depth)
        {
            std::string ind = Indent(depth);

            file << ind << "{\n";
            file << ind << "    name = \"" << prefab.GetName() << "\",\n";
            if (!prefab.GetSourceUuid().empty()) {
                file << ind << "    uuid = \"" << prefab.GetSourceUuid() << "\",\n";
            }
            if (prefab.GetCollisionLayer() != 0) {
                file << ind << "    collisionLayer = "
                    << ScenePropertySerializer::FormatString(
                        Physics::PhysicsLayerSettings::Get().GetLayerName(prefab.GetCollisionLayer()))
                    << ",\n";
            }
            if (prefab.GetStaticFlags() != Scene::StaticFlags::None) {
                file << ind << "    staticFlags = "
                    << static_cast<std::uint32_t>(prefab.GetStaticFlags()) << ",\n";
            }
            WriteTransform(file, prefab, depth + 1);
            file << ind << "    components = {\n";

            for (const Scene::ComponentSnapshot& snap : prefab.GetSnapshots())
            {
                Scene::Component* temp = Scripting::ComponentRegistry::GetInstance()
                    .CreateComponent(snap.typeName);
                if (!temp) continue;

                Scene::Prefab::ApplySnapshot(temp, snap);
                // depth + 2: one extra level for the components array, one for the component table
                Scripting::ScenePropertySerializer::WriteComponent(file, temp, depth + 2);
                Scripting::ComponentRegistry::GetInstance().DestroyComponent(snap.typeName, temp);
            }

            file << ind << "    },\n";

            const auto& children = prefab.GetChildPrefabs();
            if (!children.empty())
            {
                file << ind << "    children = {\n";
                for (const auto& child : children)
                {
                    if (!child) continue;
                    WriteNode(file, *child, depth + 2);
                    file << ",\n";
                }
                file << ind << "    },\n";
            }

            file << ind << "}";
        }

        bool PrefabSaver::Save(const Scene::Prefab& prefab, const std::string& filePath)
        {
            std::filesystem::create_directories(
                std::filesystem::path(filePath).parent_path());

            std::ofstream file(filePath);
            if (!file.is_open()) {
                RTB_ERROR("PrefabSaver: Failed to open file for writing: " + filePath);
                return false;
            }

            try {
                file << "return {\n";
                file << "    name = \"" << prefab.GetName() << "\",\n";
                if (!prefab.GetSourceUuid().empty()) {
                    file << "    uuid = \"" << prefab.GetSourceUuid() << "\",\n";
                }
                if (prefab.GetCollisionLayer() != 0) {
                    file << "    collisionLayer = "
                        << ScenePropertySerializer::FormatString(
                            Physics::PhysicsLayerSettings::Get().GetLayerName(prefab.GetCollisionLayer()))
                        << ",\n";
                }
                if (prefab.GetStaticFlags() != Scene::StaticFlags::None) {
                    file << "    staticFlags = "
                        << static_cast<std::uint32_t>(prefab.GetStaticFlags()) << ",\n";
                }
                WriteTransform(file, prefab, 1);
                file << "    components = {\n";

                for (const Scene::ComponentSnapshot& snap : prefab.GetSnapshots())
                {
                    Scene::Component* temp = Scripting::ComponentRegistry::GetInstance()
                        .CreateComponent(snap.typeName);
                    if (!temp) continue;

                    Scene::Prefab::ApplySnapshot(temp, snap);
                    Scripting::ScenePropertySerializer::WriteComponent(file, temp, 2);
                    Scripting::ComponentRegistry::GetInstance().DestroyComponent(snap.typeName, temp);
                }

                file << "    },\n";

                const auto& children = prefab.GetChildPrefabs();
                if (!children.empty())
                {
                    file << "    children = {\n";
                    for (const auto& child : children)
                    {
                        if (!child) continue;
                        WriteNode(file, *child, 2);
                        file << ",\n";
                    }
                    file << "    },\n";
                }

                file << "}\n";

                file.close();
                RTB_INFO("PrefabSaver: Saved prefab to: " + filePath);
                return true;
            }
            catch (const std::exception& e) {
                RTB_ERROR("PrefabSaver: Exception during save: " + std::string(e.what()));
                return false;
            }
        }

    }
}
