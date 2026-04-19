#pragma once

#include <btBulletDynamicsCommon.h>
#include "RigidBody.h"
#include "../Math/Vectors/Vector3.h"
#include "../Math/Quaternions/Quaternion.h"
#include <algorithm>
#include <cmath>

namespace RTBEngine {
    namespace Physics {
        namespace PhysicsUtils {
            constexpr float kPlanarDirectionEpsilon = 0.0001f;
            constexpr float kPi = 3.14159265358979323846f;
            constexpr float kRadToDeg = 180.0f / kPi;
            constexpr float kDegToRad = kPi / 180.0f;

            // Convert from engine math types to Bullet types
            inline btVector3 ToBullet(const Math::Vector3& v) {
                return btVector3(v.x, v.y, v.z);
            }

            inline btQuaternion ToBullet(const Math::Quaternion& q) {
                return btQuaternion(q.x, q.y, q.z, q.w);
            }

            // Convert from Bullet types to engine math types
            inline Math::Vector3 FromBullet(const btVector3& v) {
                return Math::Vector3(v.x(), v.y(), v.z());
            }

            inline Math::Quaternion FromBullet(const btQuaternion& q) {
                return Math::Quaternion(q.x(), q.y(), q.z(), q.w());
            }

            inline Math::Vector3 GetPlanarForwardFromRotation(const Math::Quaternion& rotation) {
                Math::Vector3 forward = rotation * Math::Vector3::Forward();
                forward.y = 0.0f;

                if (forward.LengthSquared() <= kPlanarDirectionEpsilon) {
                    return Math::Vector3::Forward();
                }

                forward.Normalize();
                return forward;
            }

            inline Math::Vector3 GetRigidBodyPlanarForward(const Physics::RigidBody* rigidBody,
                                                           const Math::Quaternion& fallbackRotation) {
                if (!rigidBody || !rigidBody->GetBulletRigidBody()) {
                    return GetPlanarForwardFromRotation(fallbackRotation);
                }

                return GetPlanarForwardFromRotation(
                    FromBullet(rigidBody->GetBulletRigidBody()->getWorldTransform().getRotation()));
            }

            inline void ConfigurePlanarDynamicBody(Physics::RigidBody* rigidBody) {
                if (!rigidBody || rigidBody->GetType() != Physics::RigidBodyType::Dynamic) {
                    return;
                }

                rigidBody->SetAngularFactor(btVector3(0.0f, 1.0f, 0.0f));

                btVector3 angularVelocity = rigidBody->GetAngularVelocity();
                angularVelocity.setX(0.0f);
                angularVelocity.setZ(0.0f);
                rigidBody->SetAngularVelocity(angularVelocity);
            }

            inline void ApplyPlanarDynamicBodyMotion(Physics::RigidBody* rigidBody,
                                                     const Math::Vector3& moveDirection,
                                                     const Math::Vector3& facingDirection,
                                                     float moveSpeed,
                                                     float turnSpeedDegrees,
                                                     float deltaTime,
                                                     const Math::Quaternion& fallbackRotation) {
                if (!rigidBody || rigidBody->GetType() != Physics::RigidBodyType::Dynamic) {
                    return;
                }

                Math::Vector3 planarMove = moveDirection;
                planarMove.y = 0.0f;
                if (planarMove.LengthSquared() > kPlanarDirectionEpsilon) {
                    planarMove.Normalize();
                } else {
                    planarMove = Math::Vector3::Zero();
                }

                Math::Vector3 planarFacing = facingDirection;
                planarFacing.y = 0.0f;
                if (planarFacing.LengthSquared() > kPlanarDirectionEpsilon) {
                    planarFacing.Normalize();
                } else {
                    planarFacing = Math::Vector3::Zero();
                }

                btVector3 velocity = rigidBody->GetLinearVelocity();
                velocity.setX(planarMove.x * std::max(0.0f, moveSpeed));
                velocity.setZ(planarMove.z * std::max(0.0f, moveSpeed));
                rigidBody->SetLinearVelocity(velocity);

                btVector3 angularVelocity = rigidBody->GetAngularVelocity();
                angularVelocity.setX(0.0f);
                angularVelocity.setZ(0.0f);

                if (planarFacing.LengthSquared() <= kPlanarDirectionEpsilon ||
                    deltaTime <= kPlanarDirectionEpsilon ||
                    turnSpeedDegrees <= 0.0f) {
                    angularVelocity.setY(0.0f);
                    rigidBody->SetAngularVelocity(angularVelocity);
                    return;
                }

                const Math::Vector3 currentForward = GetRigidBodyPlanarForward(rigidBody, fallbackRotation);
                const float signedAngleRadians = std::atan2(
                    currentForward.Cross(planarFacing).y,
                    std::clamp(currentForward.Dot(planarFacing), -1.0f, 1.0f));
                const float signedAngleDegrees = signedAngleRadians * kRadToDeg;

                if (std::abs(signedAngleDegrees) <= 0.1f) {
                    angularVelocity.setY(0.0f);
                } else {
                    const float yawSpeedDegrees =
                        std::clamp(signedAngleDegrees / deltaTime, -turnSpeedDegrees, turnSpeedDegrees);
                    angularVelocity.setY(yawSpeedDegrees * kDegToRad);
                }

                rigidBody->SetAngularVelocity(angularVelocity);
            }

        }
    }
}
