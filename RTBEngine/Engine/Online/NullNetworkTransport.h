#pragma once

#include "IOnlineTransport.h"

#include <string>

namespace RTBEngine {
    namespace Online {

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API NullNetworkTransport final : public IOnlineTransport {
        public:
            bool IsAvailable() const override;
            OnlineResult SendPacket(
                const OnlineUserId& remoteUserId,
                std::uint8_t channel,
                const void* data,
                std::uint32_t size,
                OnlinePacketReliability reliability) override;
            bool ReceivePacket(OnlinePacket& outPacket) override;
            void CloseConnections() override;
            const char* GetLastError() const override;

        private:
            std::string lastError;
        };
#pragma warning(pop)

    }
}
