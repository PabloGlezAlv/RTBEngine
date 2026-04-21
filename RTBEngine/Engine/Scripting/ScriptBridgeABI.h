#pragma once
#include <cstddef>
#include <cstdint>

// ABI-safe bridge descriptors shared by EXE and GameScripts.dll.
// Keep these POD-only and byte-for-byte identical on both sides.
static constexpr std::uint32_t RTB_SCRIPT_BRIDGE_ABI_VERSION = 1;

struct RTBPropertyDesc {
    const char* name;
    const char* displayName;
    int         type;
    size_t      offset;
    size_t      size;
    int         flags;
    float       rangeMin;
    float       rangeMax;
    int         hasRange;
    const char* assetTypeName;
    const char* componentTypeName;
};

struct RTBScriptTypeDesc {
    const char* typeName;
    void*       (*createComponent)();
    void        (*destroyComponent)(void* component);
};

struct RTBScriptBuildInfo {
    std::uint32_t abiVersion;
    int           isDebug;
    int           iteratorDebugLevel;
    size_t        sizeofStdString;
    size_t        sizeofPropertyDesc;
    size_t        sizeofScriptTypeDesc;
};
