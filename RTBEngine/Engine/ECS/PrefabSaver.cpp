#include "PrefabSaver.h"
#include "Prefab.h"
#include "../Scripting/ScenePropertySerializer.h"
#include "../RTBEngine.h"
#include <fstream>
#include <filesystem>

namespace RTBEngine {
    namespace ECS {

        bool PrefabSaver::Save(const Prefab& prefab, const std::string& filePath)
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

                for (const ComponentSnapshot& snap : prefab.GetSnapshots())
                {
                    Component* temp = Scripting::ComponentRegistry::GetInstance()
                        .CreateComponent(snap.typeName);
                    if (!temp) continue;

                    Prefab::ApplySnapshot(temp, snap);
                    Scripting::ScenePropertySerializer::WriteComponent(file, temp, 2);
                    delete temp;
                }

                file << "    }\n";
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
