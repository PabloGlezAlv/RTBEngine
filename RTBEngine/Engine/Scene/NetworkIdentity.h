#pragma once

#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Online/OnlineUser.h"
#include "../Reflection/PropertyMacros.h"

#include <cstdint>
#include <string>

namespace RTBEngine {
    namespace ECS {

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API NetworkIdentity : public Component {
        public:
            NetworkIdentity() = default;
            ~NetworkIdentity() override = default;

            std::string networkOwnerUserId;
            int networkPlayerSlot = -1;

            RTB_COMPONENT(NetworkIdentity)

        public:
            void SetOwnerUserId(const Online::OnlineUserId& userId);
            void SetNetworkPlayerSlot(int slot);

            void SetNetworkId(std::uint32_t id);
            std::uint32_t GetNetworkId() const { return networkId; }
            bool HasNetworkId() const { return networkId != 0; }

            bool IsLocallyControlled() const;
            bool IsSimulatedByHost() const;

        private:
            std::uint32_t networkId = 0;
        };
#pragma warning(pop)

    }
}
