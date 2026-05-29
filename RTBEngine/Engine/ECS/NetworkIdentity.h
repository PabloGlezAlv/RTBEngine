#pragma once

#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Online/OnlineUser.h"
#include "../Reflection/PropertyMacros.h"

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

            bool IsLocallyControlled() const;
            bool IsSimulatedByHost() const;
            std::string GetNetworkObjectKey() const;
        };
#pragma warning(pop)

    }
}
