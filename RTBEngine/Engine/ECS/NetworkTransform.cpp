#include "NetworkTransform.h"

#include "GameObject.h"
#include "NetworkIdentity.h"
#include "RigidBodyComponent.h"
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
            RTB_PROPERTY(objectKey)
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

        void NetworkTransform::OnStart()
        {
            ResolveObjectKey();
            OnValidate();
        }

        void NetworkTransform::OnFixedUpdate(float fixedDeltaTime)
        {
            if (!owner || !Online::OnlineSystem::GetInstance().IsInLobby()) {
                return;
            }

            ResolveObjectKey();

            if (HasSendAuthority()) {
                SendSnapshot(fixedDeltaTime);
            }
        }

        void NetworkTransform::OnLateUpdate(float deltaTime)
        {
            if (!owner || !Online::OnlineSystem::GetInstance().IsInLobby()) {
                return;
            }

            ResolveObjectKey();

            if (HasReceiveAuthority()) {
                ApplyRemoteSnapshot(deltaTime);
            }
        }

        void NetworkTransform::OnValidate()
        {
            sendRate = std::clamp(sendRate, 1.0f, 60.0f);
            interpolationSpeed = std::clamp(interpolationSpeed, 1.0f, 60.0f);
            ResolveObjectKey();
        }

        void NetworkTransform::ResolveObjectKey()
        {
            if (!owner) {
                resolvedObjectKey = objectKey;
                return;
            }

            if (const NetworkIdentity* identity = owner->GetComponent<NetworkIdentity>()) {
                const std::string identityKey = identity->GetNetworkObjectKey();
                if (!identityKey.empty()) {
                    resolvedObjectKey = identityKey;
                    return;
                }
            }

            if (!objectKey.empty()) {
                resolvedObjectKey = objectKey;
                return;
            }

            resolvedObjectKey = owner->GetUUID().empty()
                ? owner->GetName()
                : owner->GetUUID();
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
            if (resolvedObjectKey.empty()) {
                return;
            }

            sendTimer += std::max(0.0f, deltaTime);
            const float sendInterval = 1.0f / std::max(1.0f, sendRate);
            if (sendTimer < sendInterval) {
                return;
            }

            sendTimer = 0.0f;

            Online::OnlineGameplayNet::TransformSnapshot snapshot;
            snapshot.objectKey = resolvedObjectKey;
            snapshot.position = owner->GetTransform().GetPosition();
            snapshot.rotation = owner->GetTransform().GetRotation();
            Online::OnlineGameplayNet::BroadcastTransform(snapshot);
        }

        void NetworkTransform::ApplyRemoteSnapshot(float deltaTime)
        {
            if (resolvedObjectKey.empty()) {
                return;
            }

            Online::OnlineGameplayNet::TransformSnapshot snapshot;
            if (Online::OnlineGameplayNet::TryGetLatestTransform(resolvedObjectKey, snapshot)) {
                cachedSnapshot = snapshot;
                hasCachedSnapshot = true;
            } else if (!hasCachedSnapshot) {
                return;
            } else {
                snapshot = cachedSnapshot;
            }

            const float t = std::clamp(interpolationSpeed * std::max(0.0f, deltaTime), 0.0f, 1.0f);
            Math::Vector3 nextPosition = owner->GetTransform().GetPosition();
            Math::Quaternion nextRotation = owner->GetTransform().GetRotation();

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
