#include "NetworkTransform.h"



#include "GameObject.h"

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

            EnsureObjectKeyRegistered();

            OnValidate();

        }



        void NetworkTransform::OnDestroy()

        {

            if (!registeredObjectKey.empty()) {

                Online::OnlineGameplayNet::UnregisterTransformObjectKey(registeredObjectKey);

                registeredObjectKey.clear();

            }

        }



        void NetworkTransform::OnFixedUpdate(float fixedDeltaTime)

        {

            if (!owner || !Online::OnlineSystem::GetInstance().IsInLobby()) {

                return;

            }



            EnsureObjectKeyRegistered();



            if (HasSendAuthority()) {

                SendSnapshot(fixedDeltaTime);

            }

        }



        void NetworkTransform::OnLateUpdate(float deltaTime)

        {

            if (!owner || !Online::OnlineSystem::GetInstance().IsInLobby()) {

                return;

            }



            EnsureObjectKeyRegistered();



            if (HasReceiveAuthority()) {

                ApplyRemoteSnapshot(deltaTime);

            }

        }



        void NetworkTransform::OnValidate()

        {

            sendRate = std::clamp(sendRate, 1.0f, 60.0f);

            interpolationSpeed = std::clamp(interpolationSpeed, 1.0f, 60.0f);

            EnsureObjectKeyRegistered();

        }



        void NetworkTransform::EnsureObjectKeyRegistered()

        {

            if (!Online::OnlineSystem::GetInstance().IsInLobby()) {

                return;

            }



            if (objectKey.empty()) {

                return;

            }



            if (registeredObjectKey == objectKey) {

                return;

            }



            if (!registeredObjectKey.empty()) {

                Online::OnlineGameplayNet::UnregisterTransformObjectKey(registeredObjectKey);

                registeredObjectKey.clear();

            }



            if (!Online::OnlineGameplayNet::RegisterTransformObjectKey(objectKey)) {

                const std::string ownerName = owner ? owner->GetName() : "unknown";

                RTB_ERROR(

                    "NetworkTransform: duplicate objectKey '" + objectKey +

                    "' on '" + ownerName + "'.");

                return;

            }



            registeredObjectKey = objectKey;

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

            if (objectKey.empty()) {

                if (!loggedEmptyObjectKeyError) {

                    const std::string ownerName = owner ? owner->GetName() : "unknown";

                    RTB_ERROR(

                        "NetworkTransform: cannot send transform snapshot without objectKey on '" +

                        ownerName + "'.");

                    loggedEmptyObjectKeyError = true;

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

            snapshot.objectKey = objectKey;

            snapshot.position = owner->GetTransform().GetPosition();

            snapshot.rotation = owner->GetTransform().GetRotation();

            Online::OnlineGameplayNet::BroadcastTransform(snapshot);

        }



        void NetworkTransform::ApplyRemoteSnapshot(float deltaTime)

        {

            if (objectKey.empty()) {

                return;

            }



            Online::OnlineGameplayNet::TransformSnapshot snapshot;

            if (Online::OnlineGameplayNet::TryGetLatestTransform(objectKey, snapshot)) {

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


