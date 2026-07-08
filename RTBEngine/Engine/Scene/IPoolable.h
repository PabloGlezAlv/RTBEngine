#pragma once

#include "../RTBEngineAPI.h"

namespace RTBEngine {
    namespace ECS {

        class RTB_API IPoolable {
        public:
            virtual ~IPoolable() = default;

            virtual void OnPoolAcquire() {}
            virtual void OnPoolRelease() {}
        };

    } // namespace ECS
} // namespace RTBEngine
