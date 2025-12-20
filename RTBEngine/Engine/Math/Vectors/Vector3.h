#pragma once
#include <cmath>

namespace RTBEngine {
    namespace Math {

        class Vector3 {
        public:
            float x, y, z;

            Vector3();
            Vector3(float x, float y, float z);
            Vector3(float value);

            Vector3 operator+(const Vector3& other) const;
            Vector3 operator-(const Vector3& other) const;
            Vector3 operator*(float scalar) const;
            Vector3 operator*(const Vector3& other) const;
            Vector3 operator/(float scalar) const;

            Vector3& operator+=(const Vector3& other);
            Vector3& operator-=(const Vector3& other);
            Vector3& operator*=(float scalar);
            Vector3& operator/=(float scalar);

            Vector3 operator-() const;

            bool operator==(const Vector3& other) const;
            bool operator!=(const Vector3& other) const;

            float Dot(const Vector3& other) const;
            Vector3 Cross(const Vector3& other) const;

            float Length() const;
            float LengthSquared() const;

            Vector3 Normalized() const;
            void Normalize();

            static Vector3 Zero();
            static Vector3 One();
            static Vector3 Up();
            static Vector3 Down();
            static Vector3 Left();
            static Vector3 Right();
            static Vector3 Forward();
            static Vector3 Back();
        };

    }
}
