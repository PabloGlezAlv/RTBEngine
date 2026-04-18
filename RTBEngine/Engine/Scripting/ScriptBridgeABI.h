#pragma once
#include <cstddef>

// ABI-safe bridge descriptors shared by EXE and GameScripts.dll.
// Keep these POD-only and byte-for-byte identical on both sides.
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

