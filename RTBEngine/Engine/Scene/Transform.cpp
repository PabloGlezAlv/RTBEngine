#include "Transform.h"
#include "GameObject.h"

namespace RTBEngine {
    namespace Scene {

        Transform::Transform()
            : position(Math::Vector3::Zero())
            , rotation(Math::Quaternion::Identity())
            , scale(Math::Vector3::One())
        {
        }

        Transform::Transform(const Math::Vector3& position, const Math::Quaternion& rotation, const Math::Vector3& scale)
            : position(position)
            , rotation(rotation)
            , scale(scale)
        {
        }

        Transform::~Transform()
        {
        }

        void Transform::MarkLocalTransformDirty()
        {
            modelMatrixDirty = true;
            if (owningGameObject) {
                owningGameObject->MarkWorldMatrixDirty();
            }
        }

        void Transform::SetPosition(const Math::Vector3& position)
        {
            this->position = position;
            MarkLocalTransformDirty();
        }

        void Transform::SetRotation(const Math::Quaternion& rotation)
        {
            this->rotation = rotation;
            MarkLocalTransformDirty();
        }

        void Transform::SetRotation(const Math::Vector3& eulerAngles)
        {
            this->rotation = Math::Quaternion::FromEulerAngles(eulerAngles);
            MarkLocalTransformDirty();
        }

        void Transform::SetScale(const Math::Vector3& scale)
        {
            this->scale = scale;
            MarkLocalTransformDirty();
        }

        Math::Vector3 Transform::GetForward() const
        {
            Math::Matrix4 rotationMatrix = rotation.ToMatrix();
            // Extract forward vector (3rd column) - now points to +Z by default
            return Math::Vector3(rotationMatrix.m[8], rotationMatrix.m[9], rotationMatrix.m[10]).Normalized();
        }

        Math::Vector3 Transform::GetRight() const
        {
            Math::Matrix4 rotationMatrix = rotation.ToMatrix();
            // With negated yaw convention, right also needs to be negated
            return Math::Vector3(-rotationMatrix.m[0], -rotationMatrix.m[1], -rotationMatrix.m[2]).Normalized();
        }

        Math::Vector3 Transform::GetUp() const
        {
            Math::Matrix4 rotationMatrix = rotation.ToMatrix();
            return Math::Vector3(rotationMatrix.m[4], rotationMatrix.m[5], rotationMatrix.m[6]).Normalized();
        }

        void Transform::Translate(const Math::Vector3& translation)
        {
            position += translation;
            MarkLocalTransformDirty();
        }

        void Transform::Rotate(const Math::Quaternion& rotation)
        {
            this->rotation = rotation * this->rotation;
            MarkLocalTransformDirty();
        }

        void Transform::Rotate(const Math::Vector3& eulerAngles)
        {
            Math::Quaternion deltaRotation = Math::Quaternion::FromEulerAngles(eulerAngles);
            this->rotation = deltaRotation * this->rotation;
            MarkLocalTransformDirty();
        }

        Math::Matrix4 Transform::GetModelMatrix() const
        {
            if (modelMatrixDirty) {
                Math::Matrix4 translationMatrix = Math::Matrix4::Translate(position);
                Math::Matrix4 rotationMatrix = rotation.ToMatrix();
                Math::Matrix4 scaleMatrix = Math::Matrix4::Scale(scale);

                cachedModelMatrix = translationMatrix * rotationMatrix * scaleMatrix;
                modelMatrixDirty = false;
            }

            return cachedModelMatrix;
        }

    }
}