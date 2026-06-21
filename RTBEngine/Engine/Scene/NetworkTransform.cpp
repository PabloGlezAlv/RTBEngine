#include "NetworkTransform.h"

#include "GameObject.h"
#include "NetworkIdentity.h"
#include "RigidBodyComponent.h"
#include "../Core/Logger.h"
#include "../Math/Quaternions/Quaternion.h"
#include "../Math/Vectors/Vector3.h"
#include "../Online/OnlineGameplayNet.h"
#include "../Online/OnlineSystem.h"
#include "../Reflection/PropertyMacros.h"

#include <algorithm>

namespace RTBEngine {
    namespace ECS {

        using ThisClass = NetworkTransform;
        RTB_REGISTER_COMPONENT(NetworkTransform)
            RTB_PROPERTY_RANGE(sendRate, 1.0f, 60.0f)
            RTB_PROPERTY_RANGE(interpolationSpeed, 1.0f, 60.0f)
            RTB_PROPERTY(replicatePosition)
            RTB_PROPERTY(replicateRotation)
        RTB_END_REGISTER(NetworkTransform)

        namespace {

            Math::Vector3 LerpVector(
                const Math::Vector3& from,
                const Math::Vector3& to,
                float t)
            {
                return from + (to - from) * t;
            }

            void SyncRigidBodyIfPresent(
                GameObject* owner,
                const Math::Vector3& position,
                const Math::Quaternion& rotation)
            {
                if (!owner) {
                    return;
                }

                auto* rigidBodyComponent = owner->GetComponent<RigidBodyComponent>();
                if (!rigidBodyComponent || !rigidBodyComponent->HasRigidBody() || !rigidBodyComponent->GetRigidBody()) {
                    return;
                }

                rigidBodyComponent->GetRigidBody()->SetWorldTransform(position, rotation);
            }

        }

        std::uint32_t NetworkTransform::ResolveNetworkId() const
        {
            if (!owner) {
                return Online::OnlineGameplayNet::kInvalidNetworkObjectId;
            }

            const NetworkIdentity* identity = owner->GetComponent<NetworkIdentity>();
            return identity ? identity->GetNetworkId() : Online::OnlineGameplayNet::kInvalidNetworkObjectId;
        }

        void NetworkTransform::OnStart()
        {
            EnsureNetworkIdRegistered();
            OnValidate();
        }

        void NetworkTransform::OnDestroy()
        {
            if (registeredNetworkId != Online::OnlineGameplayNet::kInvalidNetworkObjectId) {
                Online::OnlineGameplayNet::UnregisterNetworkObjectId(registeredNetworkId);
                registeredNetworkId = Online::OnlineGameplayNet::kInvalidNetworkObjectId;
            }
        }

        void NetworkTransform::OnFixedUpdate(float fixedDeltaTime)
        {
            if (!owner || !Online::OnlineSystem::GetInstance().IsInLobby()) {
                return;
            }

            EnsureNetworkIdRegistered();
            (void)fixedDeltaTime;
        }

        void NetworkTransform::OnLateUpdate(float deltaTime)
        {
            if (!owner || !Online::OnlineSystem::GetInstance().IsInLobby()) {
                return;
            }

            EnsureNetworkIdRegistered();

            if (HasSendAuthority()) {
                SendSnapshot(deltaTime);
            }

            if (HasReceiveAuthority()) {
                ApplyRemoteSnapshot(deltaTime);
            }
        }

        void NetworkTransform::OnValidate()
        {
            sendRate = std::clamp(sendRate, 1.0f, 60.0f);
            interpolationSpeed = std::clamp(interpolationSpeed, 1.0f, 60.0f);
            EnsureNetworkIdRegistered();
        }

        void NetworkTransform::EnsureNetworkIdRegistered()
        {
            if (!Online::OnlineSystem::GetInstance().IsInLobby()) {
                return;
            }

            const std::uint32_t networkId = ResolveNetworkId();
            if (networkId == Online::OnlineGameplayNet::kInvalidNetworkObjectId) {
                return;
            }

            if (registeredNetworkId == networkId) {
                return;
            }

            if (registeredNetworkId != Online::OnlineGameplayNet::kInvalidNetworkObjectId) {
                Online::OnlineGameplayNet::UnregisterNetworkObjectId(registeredNetworkId);
                registeredNetworkId = Online::OnlineGameplayNet::kInvalidNetworkObjectId;
            }

            if (!Online::OnlineGameplayNet::RegisterNetworkObjectId(networkId)) {
                const std::string ownerName = owner ? owner->GetName() : "unknown";
                RTB_ERROR(
                    "NetworkTransform: duplicate network id " + std::to_string(networkId) +
                    " on '" + ownerName + "'.");
                return;
            }

            registeredNetworkId = networkId;
        }

        bool NetworkTransform::HasSendAuthority() const
        {
            const Online::OnlineSystem& online = Online::OnlineSystem::GetInstance();
            return !online.IsInLobby() || online.IsLobbyOwner();
        }

        bool NetworkTransform::HasReceiveAuthority() const
        {
            const Online::OnlineSystem& online = Online::OnlineSystem::GetInstance();
            return online.IsInLobby() && !online.IsLobbyOwner();
        }

        void NetworkTransform::SendSnapshot(float deltaTime)
        {
            const std::uint32_t networkId = ResolveNetworkId();
            if (networkId == Online::OnlineGameplayNet::kInvalidNetworkObjectId) {
                if (!loggedMissingNetworkIdError) {
                    const std::string ownerName = owner ? owner->GetName() : "unknown";
                    RTB_ERROR(
                        "NetworkTransform: cannot send transform snapshot without a network id on '" +
                        ownerName + "'.");
                    loggedMissingNetworkIdError = true;
                }
                return;
            }

            sendTimer += std::max(0.0f, deltaTime);
            const float sendInterval = 1.0f / std::max(1.0f, sendRate);
            if (sendTimer < sendInterval) {
                return;
            }

            sendTimer = 0.0f;

            Online::OnlineGameplayNet::TransformSnapshot snapshot;
            snapshot.networkId = networkId;
            snapshot.position = owner->GetWorldPosition();
            snapshot.rotation = owner->GetWorldRotation();
            Online::OnlineGameplayNet::BroadcastTransform(snapshot);
        }

        void NetworkTransform::ApplyRemoteSnapshot(float deltaTime)
        {
            const std::uint32_t networkId = ResolveNetworkId();
            if (networkId == Online::OnlineGameplayNet::kInvalidNetworkObjectId) {
                return;
            }

            Online::OnlineGameplayNet::TransformSnapshot snapshot;
            if (Online::OnlineGameplayNet::TryGetLatestTransform(networkId, snapshot)) {
                cachedSnapshot = snapshot;
                hasCachedSnapshot = true;
            } else if (!hasCachedSnapshot) {
                return;
            } else {
                snapshot = cachedSnapshot;
            }

            const float t = std::clamp(interpolationSpeed * std::max(0.0f, deltaTime), 0.0f, 1.0f);
            Math::Vector3 nextPosition = owner->GetWorldPosition();
            Math::Quaternion nextRotation = owner->GetWorldRotation();

            if (replicatePosition) {
                nextPosition = LerpVector(nextPosition, snapshot.position, t);
                owner->GetTransform().SetPosition(nextPosition);
            }

            if (replicateRotation) {
                nextRotation = Math::Quaternion::Slerp(nextRotation, snapshot.rotation, t);
                owner->GetTransform().SetRotation(nextRotation);
            }

            SyncRigidBodyIfPresent(owner, nextPosition, nextRotation);
        }

    }
}
