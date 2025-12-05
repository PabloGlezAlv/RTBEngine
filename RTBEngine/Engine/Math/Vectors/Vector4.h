#pragma once
#include <cmath>

namespace RTBEngine {
    namespace Math {

        class Vector4 {
        public:
            float x, y, z, w;

            Vector4();
            Vector4(float x, float y, float z, float w);
            Vector4(float value);

            Vector4 operator+(const Vector4& other) const;
            Vector4 operator-(const Vector4& other) const;
            Vector4 operator*(float scalar) const;
            Vector4 operator/(float scalar) const;

            Vector4& operator+=(const Vector4& other);
            Vector4& operator-=(const Vector4& other);
            Vector4& operator*=(float scalar);
            Vector4& operator/=(float scalar);

            Vector4 operator-() const;

            bool operator==(const Vector4& other) const;
            bool operator!=(const Vector4& other) const;

            float Dot(const Vector4& other) const;

            float Length() const;
            float LengthSquared() const;

            Vector4 Normalized() const;
            void Normalize();

            static Vector4 Zero();
            static Vector4 One();
        };

    }
}