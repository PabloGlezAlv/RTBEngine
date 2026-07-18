#pragma once

#include "../RTBEngineAPI.h"

namespace RTBEngine {
    namespace Scene {

        class RTB_API IPoolable {
        public:
            virtual ~IPoolable() = default;

            virtual void OnPoolAcquire() {}
            virtual void OnPoolRelease() {}
        };

    } // namespace Scene
} // namespace RTBEngine
