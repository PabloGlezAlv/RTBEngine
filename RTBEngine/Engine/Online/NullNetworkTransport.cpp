#include "NullNetworkTransport.h"

namespace RTBEngine {
    namespace Online {

        bool NullNetworkTransport::IsAvailable() const
        {
            return true;
        }

        OnlineResult NullNetworkTransport::SendPacket(
            const OnlineUserId&,
            std::uint8_t,
            const void*,
            std::uint32_t,
            OnlinePacketReliability)
        {
            lastError.clear();
            return OnlineResult::Success("Null transport accepted packet locally.");
        }

        bool NullNetworkTransport::ReceivePacket(OnlinePacket&)
        {
            return false;
        }

        void NullNetworkTransport::CloseConnections()
        {
            lastError.clear();
        }

        const char* NullNetworkTransport::GetLastError() const
        {
            return lastError.c_str();
        }

    }
}
