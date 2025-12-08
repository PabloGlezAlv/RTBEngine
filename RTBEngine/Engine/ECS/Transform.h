#pragma once
#include "../Math/Math.h"

namespace RTBEngine {
    namespace ECS {

        class Transform {
        public:
            Transform();
            Transform(const Math::Vector3& position, const Math::Quaternion& rotation, const Math::Vector3& scale);
            ~Transform();

            void SetPosition(const Math::Vector3& position);
            void SetRotation(const Math::Quaternion& rotation);
            void SetRotation(const Math::Vector3& eulerAngles);
            void SetScale(const Math::Vector3& scale);

            const Math::Vector3& GetPosition() const { return position; }
            const Math::Quaternion& GetRotation() const { return rotation; }
            const Math::Vector3& GetScale() const { return scale; }

            Math::Vector3 GetForward() const;
            Math::Vector3 GetRight() const;
            Math::Vector3 GetUp() const;

            void Translate(const Math::Vector3& translation);
            void Rotate(const Math::Quaternion& rotation);
            void Rotate(const Math::Vector3& eulerAngles);

            Math::Matrix4 GetModelMatrix() const;

        private:
            Math::Vector3 position;
            Math::Quaternion rotation;
            Math::Vector3 scale;
        };

    }
}