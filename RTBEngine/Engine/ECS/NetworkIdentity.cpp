#include "NetworkIdentity.h"

#include "GameObject.h"
#include "../Online/OnlineSystem.h"
#include "../Reflection/PropertyMacros.h"

namespace RTBEngine {
    namespace ECS {

        using ThisClass = NetworkIdentity;
        RTB_REGISTER_COMPONENT(NetworkIdentity)
            RTB_PROPERTY(networkOwnerUserId)
            RTB_PROPERTY(networkPlayerSlot)
        RTB_END_REGISTER(NetworkIdentity)

        void NetworkIdentity::SetOwnerUserId(const Online::OnlineUserId& userId)
        {
            networkOwnerUserId = userId.IsValid() ? userId.ToString() : std::string();
        }

        void NetworkIdentity::SetNetworkPlayerSlot(int slot)
        {
            networkPlayerSlot = slot;
        }

        bool NetworkIdentity::IsLocallyControlled() const
        {
            const Online::OnlineSystem& online = Online::OnlineSystem::GetInstance();
            if (!online.IsInLobby()) {
                return true;
            }

            if (networkPlayerSlot >= 0) {
                return static_cast<int>(online.GetLocalPlayerIndex()) == networkPlayerSlot;
            }

            if (networkOwnerUserId.empty()) {
                return true;
            }

            return networkOwnerUserId == online.GetLocalUserId().ToString();
        }

        bool NetworkIdentity::IsSimulatedByHost() const
        {
            const Online::OnlineSystem& online = Online::OnlineSystem::GetInstance();
            if (!online.IsInLobby()) {
                return true;
            }

            return online.IsLobbyOwner();
        }

        std::string NetworkIdentity::GetNetworkObjectKey() const
        {
            if (networkPlayerSlot >= 0) {
                return "PlayerSlot_" + std::to_string(networkPlayerSlot);
            }

            if (!networkOwnerUserId.empty()) {
                return networkOwnerUserId;
            }

            if (!owner) {
                return std::string();
            }

            return owner->GetUUID().empty() ? owner->GetName() : owner->GetUUID();
        }

    }
}
