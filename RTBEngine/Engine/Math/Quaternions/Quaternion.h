#pragma once
#include "../Vectors/Vector3.h"
#include "../Matrix/Matrix4.h"
#include <cmath>

namespace RTBEngine {
    namespace Math {

        class Quaternion {
        public:
            float x, y, z, w;

            Quaternion();
            Quaternion(float x, float y, float z, float w);
            Quaternion(const Vector3& axis, float angle);

            static Quaternion Identity();

            Quaternion operator+(const Quaternion& other) const;
            Quaternion operator-(const Quaternion& other) const;
            Quaternion operator*(const Quaternion& other) const;
            Vector3 operator*(const Vector3& v) const;
            Quaternion operator*(float scalar) const;

            Quaternion& operator+=(const Quaternion& other);
            Quaternion& operator-=(const Quaternion& other);
            Quaternion& operator*=(const Quaternion& other);
            Quaternion& operator*=(float scalar);

            bool operator==(const Quaternion& other) const;
            bool operator!=(const Quaternion& other) const;

            float Dot(const Quaternion& other) const;
            float Length() const;
            float LengthSquared() const;

            Quaternion Normalized() const;
            void Normalize();

            Quaternion Conjugate() const;
            Quaternion Inverse() const;

            Matrix4 ToMatrix() const;
            Vector3 ToEulerAngles() const;

            static Quaternion FromEulerAngles(float pitch, float yaw, float roll);
            static Quaternion FromEulerAngles(const Vector3& euler);
            static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);
            static Quaternion Lerp(const Quaternion& a, const Quaternion& b, float t);
        };

    }
}