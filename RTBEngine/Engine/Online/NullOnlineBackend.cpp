#include "NullOnlineBackend.h"

#include "OnlineConfig.h"
#include "../Core/Logger.h"

namespace RTBEngine {
    namespace Online {

        // The null backend follows the same lifecycle contract without touching any SDK.
        const char* NullOnlineBackend::GetName() const
        {
            return "Null";
        }

        bool NullOnlineBackend::Initialize(const OnlineConfig&)
        {
            initialized = true;
            RTB_INFO("OnlineSystem: Null backend initialized.");
            return true;
        }

        void NullOnlineBackend::Tick(float)
        {
        }

        void NullOnlineBackend::Shutdown()
        {
            if (initialized) {
                RTB_INFO("OnlineSystem: Null backend shut down.");
            }

            initialized = false;
        }

        bool NullOnlineBackend::IsInitialized() const
        {
            return initialized;
        }

        const char* NullOnlineBackend::GetLastError() const
        {
            return "";
        }

    }
}
