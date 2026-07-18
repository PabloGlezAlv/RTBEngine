#pragma once

#include "../RTBEngineAPI.h"
#include <cstdint>

namespace RTBEngine {
    namespace ECS {

        struct RTB_API Entity {
            std::uint32_t index = 0;
            std::uint32_t generation = 0;

            bool IsValid() const { return generation != 0; }

            bool operator==(const Entity& other) const
            {
                return index == other.index && generation == other.generation;
            }

            bool operator!=(const Entity& other) const
            {
                return !(*this == other);
            }
        };

        inline constexpr Entity kNullEntity{};

    }
}
