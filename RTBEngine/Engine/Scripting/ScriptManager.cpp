#include "ScriptManager.h"
#include "ScriptBridgeABI.h"
#include "../Core/Logger.h"
#include "../Scene/SceneManager.h"
#include <sstream>
#include <iomanip>
#include <cstdint>

#ifndef _ITERATOR_DEBUG_LEVEL
#define _ITERATOR_DEBUG_LEVEL 0
#endif

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
//   Used to call RTBScripts_RegisterAll exported from GameScripts.dll, which
//   bridges the DLL-side TypeRegistry into the EXE-side TypeRegistry.

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

        // State shared between the three bridge callbacks during a single type registration.
        static RTBEngine::Reflection::TypeInfo* s_pendingInfo = nullptr;
        static std::string                       s_pendingTypeName;
        static size_t                            s_pendingPropIndex = 0;
        static size_t                            s_pendingInstanceSize = 0;
        static std::vector<std::string>         s_pendingTypes;

        static int ExpectedDebugFlag()
        {
#ifdef _DEBUG
            return 1;
#else
            return 0;
#endif
        }

        static const char* BuildFlavorName(int isDebug)
        {
            return isDebug ? "Debug" : "Release";
        }

        static bool ValidateScriptBuildInfo(const RTBScriptBuildInfo* info, const std::string& dllPath)
        {
            if (!info) {
                RTB_ERROR("ScriptManager: '" + dllPath + "' did not return RTBScriptBuildInfo.");
                return false;
            }

            const int expectedDebug = ExpectedDebugFlag();
            const int expectedIteratorDebugLevel = _ITERATOR_DEBUG_LEVEL;
            const bool matches =
                info->abiVersion == RTB_SCRIPT_BRIDGE_ABI_VERSION &&
                info->isDebug == expectedDebug &&
                info->iteratorDebugLevel == expectedIteratorDebugLevel &&
                info->sizeofStdString == sizeof(std::string) &&
                info->sizeofPropertyDesc == sizeof(RTBPropertyDesc) &&
                info->sizeofScriptTypeDesc == sizeof(RTBScriptTypeDesc);

            if (matches) {
                return true;
            }

            RTB_ERROR(
                "ScriptManager: Refusing to load '" + dllPath + "' because its script ABI/build settings do not match RTBEngine. "
                "Engine{abi=" + std::to_string(RTB_SCRIPT_BRIDGE_ABI_VERSION) +
                ", config=" + BuildFlavorName(expectedDebug) +
                ", iteratorDebugLevel=" + std::to_string(expectedIteratorDebugLevel) +
                ", sizeof(std::string)=" + std::to_string(sizeof(std::string)) +
                ", sizeof(RTBPropertyDesc)=" + std::to_string(sizeof(RTBPropertyDesc)) +
                ", sizeof(RTBScriptTypeDesc)=" + std::to_string(sizeof(RTBScriptTypeDesc)) +
                "} Script{abi=" + std::to_string(info->abiVersion) +
                ", config=" + BuildFlavorName(info->isDebug) +
                ", iteratorDebugLevel=" + std::to_string(info->iteratorDebugLevel) +
                ", sizeof(std::string)=" + std::to_string(info->sizeofStdString) +
                ", sizeof(RTBPropertyDesc)=" + std::to_string(info->sizeofPropertyDesc) +
                ", sizeof(RTBScriptTypeDesc)=" + std::to_string(info->sizeofScriptTypeDesc) +
                "}.");
            return false;
        }

        static std::string PtrToHex(const void* ptr)
        {
            std::ostringstream oss;
            oss << "0x" << std::hex << std::uppercase << reinterpret_cast<std::uintptr_t>(ptr);
            return oss.str();
        }

        static std::string SafeCString(const char* value, const char* fieldName)
        {
            if (!value) {
                return "";
            }

            const std::uintptr_t rawPtr = reinterpret_cast<std::uintptr_t>(value);
            // Guard against obviously invalid addresses:
            // - null-adjacent garbage
            // - very low 32-bit-like pointers seen during corrupted bridge descriptors
            if (rawPtr < 0x10000u || rawPtr < 0x0000010000000000ull) {
                RTB_WARN(
                    "ScriptManager: Invalid C-string for field '" +
                    std::string(fieldName ? fieldName : "unknown") + "' in type '" +
                    s_pendingTypeName + "', property index " + std::to_string(s_pendingPropIndex) +
                    " ptr=" + PtrToHex(value));
                return "";
            }

            return std::string(value);
        }

        static void BridgeBeginType(const char* typeName, void* scriptTypeDesc)
        {
            auto& reg = RTBEngine::Reflection::TypeRegistry::GetInstance();
            const RTBEngine::Reflection::TypeInfo* existing = reg.GetTypeInfo(typeName);
            if (existing) {
                // Type already registered (e.g. engine built-ins) — skip.
                s_pendingInfo = nullptr;
                s_pendingTypeName.clear();
                s_pendingPropIndex = 0;
                return;
            }
            // Register the new script type in the EXE's registry.
            // The factory and destroyer delegate back into the DLL via raw function pointers.
            // No STL types cross the module boundary — all POD.
            RTBEngine::Reflection::TypeInfo newInfo(typeName);
            auto* desc = static_cast<RTBScriptTypeDesc*>(scriptTypeDesc);
            if (desc) {
                newInfo.SetFactory(
                    [](void* ctx) -> RTBEngine::ECS::Component* {
                        auto* scriptDesc = static_cast<RTBScriptTypeDesc*>(ctx);
                        if (!scriptDesc || !scriptDesc->createComponent) {
                            return nullptr;
                        }
                        return static_cast<RTBEngine::ECS::Component*>(scriptDesc->createComponent());
                    },
                    static_cast<void*>(desc));
                // The destroyer must run inside GameScripts.dll so that delete uses
                // the same module/runtime that new used.
                newInfo.SetDestroyer(
                    [](RTBEngine::ECS::Component* c, void* ctx) {
                        auto* scriptDesc = static_cast<RTBScriptTypeDesc*>(ctx);
                        if (scriptDesc && scriptDesc->destroyComponent) {
                            scriptDesc->destroyComponent(static_cast<void*>(c));
                        }
                    },
                    static_cast<void*>(desc));
            }
            reg.RegisterType(typeName, newInfo);
            s_pendingInfo = const_cast<RTBEngine::Reflection::TypeInfo*>(reg.GetTypeInfo(typeName));
            s_pendingTypeName = typeName ? typeName : "";
            s_pendingPropIndex = 0;
            s_pendingInstanceSize = (desc && desc->instanceSize > 0) ? desc->instanceSize : 0;
            s_pendingTypes.push_back(typeName);
        }

        static void BridgePropCallback(const RTBPropertyDesc* desc)
        {
            if (!s_pendingInfo || !desc) return;
            constexpr size_t kMaxBridgedPropsPerType = 128;
            if (s_pendingPropIndex >= kMaxBridgedPropsPerType) {
                RTB_WARN(
                    "ScriptManager: Reached property safety limit (" + std::to_string(kMaxBridgedPropsPerType) +
                    ") while bridging type '" + s_pendingTypeName + "'. Remaining properties are ignored.");
                return;
            }

            RTBEngine::Reflection::PropertyInfo prop;
            prop.name              = SafeCString(desc->name, "name");
            prop.displayName       = SafeCString(desc->displayName, "displayName");
            prop.type              = static_cast<RTBEngine::Reflection::PropertyType>(desc->type);
            prop.offset            = desc->offset;
            prop.size              = desc->size;
            prop.flags             = static_cast<RTBEngine::Reflection::PropertyFlags>(desc->flags);
            if (prop.type == RTBEngine::Reflection::PropertyType::AssetRef) {
                prop.assetType = SafeCString(desc->assetTypeName, "assetTypeName");
            }
            if (prop.type == RTBEngine::Reflection::PropertyType::ComponentRef) {
                prop.componentTypeName = SafeCString(desc->componentTypeName, "componentTypeName");
            }
            if (prop.type == RTBEngine::Reflection::PropertyType::List) {
                prop.listElementType = static_cast<RTBEngine::Reflection::ListElementType>(desc->listElementType);
                if (prop.listElementType == RTBEngine::Reflection::ListElementType::ComponentRef) {
                    prop.componentTypeName = SafeCString(desc->componentTypeName, "componentTypeName");
                }
            }
            if (desc->hasRange) {
                prop.range = RTBEngine::Reflection::Range(desc->rangeMin, desc->rangeMax);
            }

            if (prop.name.empty()) {
                RTB_WARN(
                    "ScriptManager: Skipping bridged property with empty name in type '" +
                    s_pendingTypeName + "', property index " + std::to_string(s_pendingPropIndex));
                ++s_pendingPropIndex;
                return;
            }

            if (s_pendingInstanceSize > 0 && prop.offset + prop.size > s_pendingInstanceSize) {
                RTB_ERROR(
                    "ScriptManager: Property '" + prop.name + "' on type '" + s_pendingTypeName +
                    "' is out of bounds (offset=" + std::to_string(prop.offset) +
                    ", size=" + std::to_string(prop.size) +
                    ", instanceSize=" + std::to_string(s_pendingInstanceSize) +
                    "). Rebuild GameScripts against the current RTBEngine SDK headers.");
                ++s_pendingPropIndex;
                return;
            }

            s_pendingInfo->AddProperty(prop);
            ++s_pendingPropIndex;
        }

        static void BridgeEndType()
        {
            s_pendingInfo = nullptr;
            s_pendingTypeName.clear();
            s_pendingPropIndex = 0;
            s_pendingInstanceSize = 0;
        }

        void ScriptManager::ScriptTypeReceiver(const char* /*name*/, const RTBEngine::Reflection::TypeInfo& /*info*/)
        {
            // Unused — replaced by the three-callback ABI-safe bridge.
        }

        bool ScriptManager::LoadScripts(const std::string& dllPath)
        {
            if (dllHandle != nullptr) {
                UnloadScripts();
            }

            // LoadLibraryA maps the DLL and runs all static initializers inside it.
            // RTB_END_REGISTER statics register components into the DLL's own copy
            // of TypeRegistry (separate from the EXE's due to static lib linkage).
            dllHandle = LoadLibraryA(dllPath.c_str());
            if (dllHandle == nullptr) {
                DWORD error = GetLastError();
                RTB_ERROR("ScriptManager: Failed to load '" + dllPath + "' (error " + std::to_string(error) + ")");
                return false;
            }

            using BuildInfoFunc = const RTBScriptBuildInfo*(*)();
            auto* getBuildInfo = reinterpret_cast<BuildInfoFunc>(
                GetProcAddress(dllHandle, "RTBScripts_GetBuildInfo"));
            if (!getBuildInfo) {
                RTB_ERROR("ScriptManager: Refusing to load '" + dllPath +
                    "' because it does not export RTBScripts_GetBuildInfo.");
                FreeLibrary(dllHandle);
                dllHandle = nullptr;
                loadedPath.clear();
                loadedScriptTypes.clear();
                return false;
            }

            if (!ValidateScriptBuildInfo(getBuildInfo(), dllPath)) {
                FreeLibrary(dllHandle);
                dllHandle = nullptr;
                loadedPath.clear();
                loadedScriptTypes.clear();
                return false;
            }

            // Bridge DLL types into the EXE registry using a POD-only ABI.
            // Three C callbacks receive primitive data; no STL crosses the boundary.
            using BridgeFunc = void(*)(
                void(*)(const char*, void*),
                void(*)(const RTBPropertyDesc*),
                void(*)());
            auto* bridge = reinterpret_cast<BridgeFunc>(
                GetProcAddress(dllHandle, "RTBScripts_RegisterAll"));
            if (bridge) {
                loadedScriptTypes.clear();
                s_pendingTypes.clear();
                bridge(&BridgeBeginType, &BridgePropCallback, &BridgeEndType);
                loadedScriptTypes = std::move(s_pendingTypes);
                // if (loadedScriptTypes.empty()) {
                //     RTB_INFO("ScriptManager: No script components registered in '" + dllPath + "'");
                // } else {
                //     for (const auto& name : loadedScriptTypes) {
                //         RTB_INFO("ScriptManager: Registered component '" + name + "'");
                //     }
                // }
            }

            loadedPath = dllPath;
            RTB_INFO("ScriptManager: Loaded '" + dllPath + "'");
            return true;
        }

        void ScriptManager::UnloadScripts()
        {
            auto& sceneManager = RTBEngine::ECS::SceneManager::GetInstance();
            sceneManager.ClearPendingSceneLoad();

            if (dllHandle == nullptr) {
                return;
            }

            // Unload the current scene first so all script components are destroyed
            // while the DLL is still mapped. FreeLibrary invalidates their vtables.
            sceneManager.UnloadCurrentScene();

            // Remove all types that came from this DLL before freeing it.
            // Component factories inside the DLL become dangling pointers after FreeLibrary.
            for (const auto& typeName : loadedScriptTypes) {
                RTBEngine::Reflection::TypeRegistry::GetInstance().UnregisterType(typeName);
            }
            loadedScriptTypes.clear();

            // FreeLibrary runs DllMain(DLL_PROCESS_DETACH) and destroys static objects.
            FreeLibrary(dllHandle);
            dllHandle = nullptr;

            RTB_INFO("ScriptManager: Unloaded '" + loadedPath + "'");
            loadedPath.clear();
        }

    }
}
