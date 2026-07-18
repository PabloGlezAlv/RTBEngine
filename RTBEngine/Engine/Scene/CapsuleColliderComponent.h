#pragma once
#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Math/Vectors/Vector3.h"
#include "../Reflection/PropertyMacros.h"
#include <memory>

namespace RTBEngine {
    namespace Physics {
        class CapsuleCollider;
        class PhysicsWorld;
    }
}

class btCollisionObject;

namespace RTBEngine {
    namespace Scene {

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API CapsuleColliderComponent : public Component {
        public:
            CapsuleColliderComponent();
            virtual ~CapsuleColliderComponent();

            CapsuleColliderComponent(const CapsuleColliderComponent&) = delete;
            CapsuleColliderComponent& operator=(const CapsuleColliderComponent&) = delete;

            void OnDestroy() override;
            void OnValidate() override;

            void SetRadius(float value);
            float GetRadius() const;

            void SetHeight(float value);
            float GetHeight() const;

            void SetCenterOffset(const Math::Vector3& offset);
            Math::Vector3 GetCenterOffset() const;

            void SetIsTrigger(bool trigger);
            bool IsTrigger() const { return isTrigger; }

            Physics::CapsuleCollider* GetCapsuleCollider() const { return capsuleCollider.get(); }
            void SetBulletCollisionObject(btCollisionObject* obj, bool takeOwnership = false);
            btCollisionObject* GetBulletCollisionObject() const { return bulletObject; }
            void SetPhysicsWorld(Physics::PhysicsWorld* world) { physicsWorldRef = world; }
            Physics::PhysicsWorld* GetPhysicsWorld() const { return physicsWorldRef; }

            float radius = 0.35f;
            float height = 1.80f;
            Math::Vector3 centerOffset = Math::Vector3(0.0f, 0.90f, 0.0f);
            bool isTrigger = false;

            RTB_COMPONENT(CapsuleColliderComponent)

        private:
            std::unique_ptr<Physics::CapsuleCollider> capsuleCollider;
            btCollisionObject* bulletObject = nullptr;
            bool ownsBulletObject = false;
            Physics::PhysicsWorld* physicsWorldRef = nullptr;
        };
#pragma warning(pop)

    }
}
