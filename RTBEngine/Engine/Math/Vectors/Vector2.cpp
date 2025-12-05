#include "Vector2.h"

namespace RTBEngine {
    namespace Math {

        Vector2::Vector2() : x(0.0f), y(0.0f) {}

        Vector2::Vector2(float x, float y) : x(x), y(y) {}

        Vector2::Vector2(float value) : x(value), y(value) {}

        Vector2 Vector2::operator+(const Vector2& other) const {
            return Vector2(x + other.x, y + other.y);
        }

        Vector2 Vector2::operator-(const Vector2& other) const {
            return Vector2(x - other.x, y - other.y);
        }

        Vector2 Vector2::operator*(float scalar) const {
            return Vector2(x * scalar, y * scalar);
        }

        Vector2 Vector2::operator/(float scalar) const {
            return Vector2(x / scalar, y / scalar);
        }

        Vector2& Vector2::operator+=(const Vector2& other) {
            x += other.x;
            y += other.y;
            return *this;
        }

        Vector2& Vector2::operator-=(const Vector2& other) {
            x -= other.x;
            y -= other.y;
            return *this;
        }

        Vector2& Vector2::operator*=(float scalar) {
            x *= scalar;
            y *= scalar;
            return *this;
        }

        Vector2& Vector2::operator/=(float scalar) {
            x /= scalar;
            y /= scalar;
            return *this;
        }

        Vector2 Vector2::operator-() const {
            return Vector2(-x, -y);
        }

        bool Vector2::operator==(const Vector2& other) const {
            return x == other.x && y == other.y;
        }

        bool Vector2::operator!=(const Vector2& other) const {
            return !(*this == other);
        }

        float Vector2::Dot(const Vector2& other) const {
            return x * other.x + y * other.y;
        }

        float Vector2::Length() const {
            return std::sqrt(x * x + y * y);
        }

        float Vector2::LengthSquared() const {
            return x * x + y * y;
        }

        Vector2 Vector2::Normalized() const {
            float len = Length();
            if (len > 0.0f) {
                return *this / len;
            }
            return Vector2::Zero();
        }

        void Vector2::Normalize() {
            float len = Length();
            if (len > 0.0f) {
                x /= len;
                y /= len;
            }
        }

        Vector2 Vector2::Zero() {
            return Vector2(0.0f, 0.0f);
        }

        Vector2 Vector2::One() {
            return Vector2(1.0f, 1.0f);
        }

        Vector2 Vector2::Up() {
            return Vector2(0.0f, 1.0f);
        }

        Vector2 Vector2::Down() {
            return Vector2(0.0f, -1.0f);
        }

        Vector2 Vector2::Left() {
            return Vector2(-1.0f, 0.0f);
        }

        Vector2 Vector2::Right() {
            return Vector2(1.0f, 0.0f);
        }

    }
}