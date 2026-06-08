#pragma once

#include "../RTBEngineAPI.h"

#include <cstdint>

namespace RTBEngine {
    namespace Online {

        // Avoid enumerator names LAN/Relay: Windows/third-party headers may define them as macros.
        enum class OnlineBackendType : std::uint8_t {
            Lan = 0,
            RelayOnline = 1
        };

        constexpr OnlineBackendType kLanBackend = OnlineBackendType::Lan;
        constexpr OnlineBackendType kRelayBackend = OnlineBackendType::RelayOnline;

        inline bool IsLanBackend(OnlineBackendType backend) noexcept
        {
            return backend == OnlineBackendType::Lan;
        }

        inline bool IsRelayBackend(OnlineBackendType backend) noexcept
        {
            return backend == OnlineBackendType::RelayOnline;
        }

        enum class OnlineState {
            Disabled,
            Initialized,
            Error
        };

        RTB_API const char* ToString(OnlineBackendType backend);
        RTB_API const char* ToString(OnlineState state);

    }
}
