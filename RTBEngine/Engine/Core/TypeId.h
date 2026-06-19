#pragma once

#include "../RTBEngineAPI.h"
#include <cstdint>

namespace RTBEngine {
    namespace TypeId {

        constexpr std::uint32_t kFNVOffsetBasis = 2166136261u;
        constexpr std::uint32_t kFNVPrime = 16777619u;

        constexpr std::uint32_t HashChar(std::uint32_t hash, char character)
        {
            return (hash ^ static_cast<std::uint32_t>(character)) * kFNVPrime;
        }

        constexpr std::uint32_t Hash(const char* text, std::uint32_t hash = kFNVOffsetBasis)
        {
            return (text != nullptr && *text != '\0')
                ? Hash(text + 1, HashChar(hash, *text))
                : hash;
        }

    } // namespace TypeId
} // namespace RTBEngine

#define RTB_TYPE_ID(ClassName) RTBEngine::TypeId::Hash(#ClassName)
