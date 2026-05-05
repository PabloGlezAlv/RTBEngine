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
            // Mark the no-op backend as ready without contacting any service.
            initialized = true;
            RTB_INFO("OnlineSystem: Null backend initialized.");
            return true;
        }

        void NullOnlineBackend::Tick(float)
        {
        }

        void NullOnlineBackend::Shutdown()
        {
            // Report shutdown only when the backend was previously initialized.
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

        IOnlineIdentity* NullOnlineBackend::GetIdentity()
        {
            // Expose the offline identity implementation.
            return &identity;
        }

        const IOnlineIdentity* NullOnlineBackend::GetIdentity() const
        {
            return &identity;
        }

    }
}
