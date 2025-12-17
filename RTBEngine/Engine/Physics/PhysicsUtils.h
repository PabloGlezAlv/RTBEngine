#pragma once

#include <btBulletDynamicsCommon.h>
#include "../Math/Vectors/Vector3.h"
#include "../Math/Quaternions/Quaternion.h"

namespace RTBEngine {
    namespace Physics {
        namespace PhysicsUtils {

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

        }
    }
}
