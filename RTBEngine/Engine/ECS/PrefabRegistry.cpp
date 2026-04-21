#include "PrefabRegistry.h"
#include "Prefab.h"
#include "../Scripting/PrefabLoader.h"
#include "../RTBEngine.h"
#include <filesystem>

namespace RTBEngine {
    namespace ECS {

        PrefabRegistry& PrefabRegistry::GetInstance()
        {
            static PrefabRegistry instance;
            return instance;
        }

        PrefabRegistry::PrefabRegistry() = default;

        PrefabRegistry::~PrefabRegistry() = default;

        void PrefabRegistry::LoadAll(const std::string& directory)
        {
            if (!std::filesystem::exists(directory)) return;

            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
            {
                if (entry.path().extension() == ".prefab")
                    Register(entry.path().string());
            }
        }

        void PrefabRegistry::Register(const std::string& filePath)
        {
            auto prefab = Scripting::PrefabLoader::Load(filePath);
            if (!prefab)
            {
                RTB_ERROR("PrefabRegistry: Failed to load prefab: " + filePath);
                return;
            }

            std::string name = prefab->GetName();
            nameToPaths[name] = filePath;
            prefabs[name] = std::move(prefab);

            if (onPrefabChanged)
                onPrefabChanged(name);
        }

        void PrefabRegistry::Unload(const std::string& name)
        {
            prefabs.erase(name);
            nameToPaths.erase(name);
        }

        void PrefabRegistry::Reload(const std::string& name)
        {
            auto it = nameToPaths.find(name);
            if (it == nameToPaths.end())
            {
                RTB_WARN("PrefabRegistry: Cannot reload unknown prefab: " + name);
                return;
            }

            std::string path = it->second;
            Unload(name);
            Register(path);
        }

        Prefab* PrefabRegistry::Get(const std::string& name) const
        {
            auto it = prefabs.find(name);
            return it != prefabs.end() ? it->second.get() : nullptr;
        }

        Prefab* PrefabRegistry::GetByPath(const std::string& filePath) const
        {
            std::filesystem::path canonical;
            std::error_code ec;
            canonical = std::filesystem::weakly_canonical(filePath, ec);

            for (const auto& [name, path] : nameToPaths)
            {
                std::filesystem::path registeredCanonical = std::filesystem::weakly_canonical(path, ec);
                if (registeredCanonical == canonical)
                    return Get(name);
            }
            return nullptr;
        }

        bool PrefabRegistry::Has(const std::string& name) const
        {
            return prefabs.find(name) != prefabs.end();
        }

        void PrefabRegistry::Clear()
        {
            prefabs.clear();
            nameToPaths.clear();
        }

    }
}
