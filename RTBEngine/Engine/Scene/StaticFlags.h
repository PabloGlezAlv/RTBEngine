#pragma once

#include "../RTBEngineAPI.h"
#include <cstdint>

namespace RTBEngine {
    namespace Scene {

        enum class StaticFlags : std::uint32_t {
            None = 0,
            Batching = 1u << 0,
            ContributeGI = 1u << 1,
            Occluder = 1u << 2,
            Navigation = 1u << 3,
            All = Batching | ContributeGI | Occluder | Navigation,
        };

        inline constexpr StaticFlags operator|(StaticFlags lhs, StaticFlags rhs)
        {
            return static_cast<StaticFlags>(
                static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
        }

        inline constexpr StaticFlags operator&(StaticFlags lhs, StaticFlags rhs)
        {
            return static_cast<StaticFlags>(
                static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
        }

        inline constexpr StaticFlags& operator|=(StaticFlags& lhs, StaticFlags rhs)
        {
            lhs = lhs | rhs;
            return lhs;
        }

        inline constexpr StaticFlags& operator&=(StaticFlags& lhs, StaticFlags rhs)
        {
            lhs = lhs & rhs;
            return lhs;
        }

        inline constexpr StaticFlags operator~(StaticFlags flags)
        {
            return static_cast<StaticFlags>(~static_cast<std::uint32_t>(flags));
        }

        inline constexpr bool HasStaticFlag(StaticFlags flags, StaticFlags flag)
        {
            return (flags & flag) != StaticFlags::None;
        }

        inline constexpr bool IsAnythingStatic(StaticFlags flags)
        {
            return flags != StaticFlags::None;
        }

        inline constexpr std::uint32_t kStaticFlagsAll =
            static_cast<std::uint32_t>(StaticFlags::All);

    }
}
