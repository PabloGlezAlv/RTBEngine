#pragma once
#include "../RTBEngineAPI.h"
#include <btBulletDynamicsCommon.h>
#include <cstdint>
#include <memory>
#include <vector>
#include "../Math/Vectors/Vector3.h"

namespace RTBEngine {
    namespace Scene {
        class GameObject;
    }

    namespace Physics {

        enum class RTB_API PhysicsDebugQueryType : std::uint8_t {
            Raycast = 0,
            SphereCast = 1
        };

        struct RTB_API OverlapSphereHit {
            Scene::GameObject* gameObject = nullptr;
            Math::Vector3 point = Math::Vector3(0.0f, 0.0f, 0.0f);
            Math::Vector3 normal = Math::Vector3(0.0f, 1.0f, 0.0f);
            float distance = 0.0f;
        };

        struct RTB_API PhysicsQueryHit {
            Scene::GameObject* gameObject = nullptr;
            Math::Vector3 point = Math::Vector3(0.0f, 0.0f, 0.0f);
            Math::Vector3 normal = Math::Vector3(0.0f, 1.0f, 0.0f);
            float fraction = 0.0f;
        };

        struct RTB_API PhysicsQueryOptions {
            const Scene::GameObject* ignoredObject = nullptr;
            bool ignoreIgnoredObjectHierarchy = true;
            bool ignoreTriggers = false;
            std::uint32_t layerMask = 0xFFFFFFFFu;
        };

        struct RTB_API PhysicsDebugQueryEntry {
            PhysicsDebugQueryType type = PhysicsDebugQueryType::Raycast;
            Math::Vector3 start = Math::Vector3(0.0f, 0.0f, 0.0f);
            Math::Vector3 end = Math::Vector3(0.0f, 0.0f, 0.0f);
            float radius = 0.0f;
            bool hit = false;
            float hitFraction = 1.0f;
            float remainingSeconds = 0.0f;
        };

        RTB_API void SetPhysicsDebugQueriesEnabled(bool enabled);
        RTB_API bool ArePhysicsDebugQueriesEnabled();
        RTB_API void ClearPhysicsDebugQueries();
        RTB_API int GetPhysicsDebugQuerySnapshot(PhysicsDebugQueryEntry* outEntries, int maxEntries);

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API PhysicsWorld {
        public:
            PhysicsWorld();
            ~PhysicsWorld();

            void Initialize();
            void Step(float deltaTime);
            void Cleanup();
            void ResetObjects();

            void AddRigidBody(btRigidBody* body);
            void AddRigidBody(btRigidBody* body, int collisionFilterGroup, int collisionFilterMask);
            void RemoveRigidBody(btRigidBody* body);

            // For static colliders without RigidBody
            void AddCollisionObject(btCollisionObject* obj);
            void AddCollisionObject(btCollisionObject* obj, int collisionFilterGroup, int collisionFilterMask);
            void RemoveCollisionObject(btCollisionObject* obj);

            void SetGravity(const Math::Vector3& gravity);
            Math::Vector3 GetGravity() const;

            bool RaycastClosest(const Math::Vector3& start,
                                const Math::Vector3& end,
                                PhysicsQueryHit& outHit,
                                const PhysicsQueryOptions& options = PhysicsQueryOptions{}) const;
            bool SphereCastClosest(const Math::Vector3& start,
                                   const Math::Vector3& end,
                                   float radius,
                                   PhysicsQueryHit& outHit,
                                   const PhysicsQueryOptions& options = PhysicsQueryOptions{}) const;

            std::vector<OverlapSphereHit> OverlapSphere(const Math::Vector3& center,
                                                        float radius,
                                                        std::uint32_t layerMask = 0xFFFFFFFFu,
                                                        const PhysicsQueryOptions& options = PhysicsQueryOptions{}) const;
            std::vector<OverlapSphereHit> OverlapCapsuleSegment(const Math::Vector3& start,
                                                                const Math::Vector3& end,
                                                                float radius,
                                                                std::uint32_t layerMask = 0xFFFFFFFFu,
                                                                const PhysicsQueryOptions& options = PhysicsQueryOptions{}) const;

            btDynamicsWorld* GetDynamicsWorld() const { return dynamicsWorld.get(); }
            btDispatcher* GetDispatcher() const { return dispatcher.get(); }
            int GetActiveBodyCount() const { return dynamicsWorld ? dynamicsWorld->getNumCollisionObjects() : 0; }

        private:
            std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration;
            std::unique_ptr<btCollisionDispatcher> dispatcher;
            std::unique_ptr<btBroadphaseInterface> broadphase;
            std::unique_ptr<btSequentialImpulseConstraintSolver> solver;
            std::unique_ptr<btDiscreteDynamicsWorld> dynamicsWorld;
        };
#pragma warning(pop)

    }
}
