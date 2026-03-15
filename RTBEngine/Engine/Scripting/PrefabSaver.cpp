#include "PrefabSaver.h"
#include "../ECS/Prefab.h"
#include "../ECS/Component.h"
#include "ComponentRegistry.h"
#include "ScenePropertySerializer.h"
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

        static void WriteNode(std::ofstream& file, const ECS::Prefab& prefab, int depth)
        {
            std::string ind = Indent(depth);

            file << ind << "{\n";
            file << ind << "    name = \"" << prefab.GetName() << "\",\n";
            file << ind << "    components = {\n";

            for (const ECS::ComponentSnapshot& snap : prefab.GetSnapshots())
            {
                ECS::Component* temp = Scripting::ComponentRegistry::GetInstance()
                    .CreateComponent(snap.typeName);
                if (!temp) continue;

                ECS::Prefab::ApplySnapshot(temp, snap);
                // depth + 2: one extra level for the components array, one for the component table
                Scripting::ScenePropertySerializer::WriteComponent(file, temp, depth + 2);
                delete temp;
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

        bool PrefabSaver::Save(const ECS::Prefab& prefab, const std::string& filePath)
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
                file << "    components = {\n";

                for (const ECS::ComponentSnapshot& snap : prefab.GetSnapshots())
                {
                    ECS::Component* temp = Scripting::ComponentRegistry::GetInstance()
                        .CreateComponent(snap.typeName);
                    if (!temp) continue;

                    ECS::Prefab::ApplySnapshot(temp, snap);
                    Scripting::ScenePropertySerializer::WriteComponent(file, temp, 2);
                    delete temp;
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
