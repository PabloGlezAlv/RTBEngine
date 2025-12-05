#pragma once
#include <cmath>

namespace RTBEngine {
    namespace Math {

        class Vector2 {
        public:
            float x, y;

            Vector2();
            Vector2(float x, float y);
            Vector2(float value);

            Vector2 operator+(const Vector2& other) const;
            Vector2 operator-(const Vector2& other) const;
            Vector2 operator*(float scalar) const;
            Vector2 operator/(float scalar) const;

            Vector2& operator+=(const Vector2& other);
            Vector2& operator-=(const Vector2& other);
            Vector2& operator*=(float scalar);
            Vector2& operator/=(float scalar);

            Vector2 operator-() const;

            bool operator==(const Vector2& other) const;
            bool operator!=(const Vector2& other) const;

            float Dot(const Vector2& other) const;

            float Length() const;
            float LengthSquared() const;

            Vector2 Normalized() const;
            void Normalize();

            static Vector2 Zero();
            static Vector2 One();
            static Vector2 Up();
            static Vector2 Down();
            static Vector2 Left();
            static Vector2 Right();
        };

    }
}