#pragma once

#include "IOnlineTransport.h"

#include <memory>

namespace RTBEngine {
    namespace Online {

        class IOnlineIdentity;

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API EosP2PTransport final : public IOnlineTransport {
        public:
            explicit EosP2PTransport(IOnlineIdentity* identity);
            ~EosP2PTransport() override;

            void SetPlatformHandle(void* handle);
            void ResetPlatformHandle();

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
            class Impl;
            std::unique_ptr<Impl> impl;
        };
#pragma warning(pop)

    }
}
