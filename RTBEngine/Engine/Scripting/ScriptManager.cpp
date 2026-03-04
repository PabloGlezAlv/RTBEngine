#include "ScriptManager.h"
#include "../Core/Logger.h"

// --- Windows DLL loading API ---
//
// HMODULE
//   A handle (opaque pointer) that represents a loaded DLL in the current process.
//   Returned by LoadLibraryA and required by FreeLibrary / GetProcAddress.
//
// LoadLibraryA(path)
//   Loads a DLL into the calling process's address space.
//   Windows maps the DLL into memory, runs DllMain(DLL_PROCESS_ATTACH), and
//   executes all static-storage initializers in the DLL — this is the moment
//   our RTB_END_REGISTER blocks run and register components in TypeRegistry.
//   Returns nullptr on failure; call GetLastError() for the Win32 error code.
//   The 'A' suffix means it accepts a narrow (char*) path string.
//
// FreeLibrary(hModule)
//   Decrements the DLL's reference count. When it reaches zero Windows runs
//   DllMain(DLL_PROCESS_DETACH), destroys static objects inside the DLL, and
//   unmaps it from memory. After this call the HMODULE is invalid.
//   Any function pointer retrieved via GetProcAddress becomes a dangling pointer.
//
// GetLastError()
//   Returns the last Win32 error code set by the calling thread.
//   Must be called immediately after the failing API — other calls may reset it.
//   Common codes: 126 = ERROR_MOD_NOT_FOUND (DLL or a dependency not found),
//                 193 = ERROR_BAD_EXE_FORMAT (wrong architecture, e.g. 32-bit vs 64-bit).
//
// GetProcAddress(hModule, name)
//   Resolves the address of an exported symbol by name at runtime.
//   Not used here — component registration happens automatically via static
//   initializers when the DLL is loaded, so no explicit entry point is needed.

namespace RTBEngine {
    namespace Scripting {

        ScriptManager& ScriptManager::GetInstance()
        {
            static ScriptManager instance;
            return instance;
        }

        ScriptManager::~ScriptManager()
        {
            UnloadScripts();
        }

        bool ScriptManager::LoadScripts(const std::string& dllPath)
        {
            if (dllHandle != nullptr) {
                UnloadScripts();
            }

            // LoadLibraryA maps the DLL and runs all static initializers inside it.
            // RTB_END_REGISTER uses a static object whose constructor calls
            // TypeRegistry::RegisterType — so components self-register here.
            dllHandle = LoadLibraryA(dllPath.c_str());
            if (dllHandle == nullptr) {
                DWORD error = GetLastError();
                RTB_ERROR("ScriptManager: Failed to load '" + dllPath + "' (error " + std::to_string(error) + ")");
                return false;
            }

            loadedPath = dllPath;
            RTB_INFO("ScriptManager: Loaded '" + dllPath + "'");
            return true;
        }

        void ScriptManager::UnloadScripts()
        {
            if (dllHandle == nullptr) {
                return;
            }

            // FreeLibrary runs DllMain(DLL_PROCESS_DETACH) and destroys static objects.
            // After this, all component factories registered from this DLL become
            // dangling pointers — the caller must clear TypeRegistry entries first
            // if hot-reloading is needed.
            FreeLibrary(dllHandle);
            dllHandle = nullptr;

            RTB_INFO("ScriptManager: Unloaded '" + loadedPath + "'");
            loadedPath.clear();
        }

    }
}
