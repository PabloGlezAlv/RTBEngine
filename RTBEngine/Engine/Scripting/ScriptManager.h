#pragma once
#include "../RTBEngineAPI.h"
#include <string>
#include <Windows.h>

namespace RTBEngine {
    namespace Scripting {

        #pragma warning(push)
        #pragma warning(disable: 4251)
        class RTB_API ScriptManager {
        public:
            static ScriptManager& GetInstance();

            ScriptManager(const ScriptManager&) = delete;
            ScriptManager& operator=(const ScriptManager&) = delete;

            //Loads GameScripts.dll and triggers static component registration
            bool LoadScripts(const std::string& dllPath);

            //Unloads the current GameScripts.dll
            void UnloadScripts();

            bool IsLoaded() const { return dllHandle != nullptr; }
            const std::string& GetLoadedPath() const { return loadedPath; }

        private:
            ScriptManager() = default;
            ~ScriptManager();

            HMODULE dllHandle = nullptr;
            std::string loadedPath;
        };
        #pragma warning(pop)

    }
}
