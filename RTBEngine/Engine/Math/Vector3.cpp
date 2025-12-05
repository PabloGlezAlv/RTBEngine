#include "Vector3.h"

namespace RTBEngine {
    namespace Math {

        Vector3::Vector3() : x(0.0f), y(0.0f), z(0.0f) {}

        Vector3::Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

        Vector3::Vector3(float value) : x(value), y(value), z(value) {}

        Vector3 Vector3::operator+(const Vector3& other) const {
            return Vector3(x + other.x, y + other.y, z + other.z);
        }

        Vector3 Vector3::operator-(const Vector3& other) const {
            return Vector3(x - other.x, y - other.y, z - other.z);
        }

        Vector3 Vector3::operator*(float scalar) const {
            return Vector3(x * scalar, y * scalar, z * scalar);
        }

        Vector3 Vector3::operator/(float scalar) const {
            return Vector3(x / scalar, y / scalar, z / scalar);
        }

        Vector3& Vector3::operator+=(const Vector3& other) {
            x += other.x;
            y += other.y;
            z += other.z;
            return *this;
        }

        Vector3& Vector3::operator-=(const Vector3& other) {
            x -= other.x;
            y -= other.y;
            z -= other.z;
            return *this;
        }

        Vector3& Vector3::operator*=(float scalar) {
            x *= scalar;
            y *= scalar;
            z *= scalar;
            return *this;
        }

        Vector3& Vector3::operator/=(float scalar) {
            x /= scalar;
            y /= scalar;
            z /= scalar;
            return *this;
        }

        Vector3 Vector3::operator-() const {
            return Vector3(-x, -y, -z);
        }

        bool Vector3::operator==(const Vector3& other) const {
            return x == other.x && y == other.y && z == other.z;
        }

        bool Vector3::operator!=(const Vector3& other) const {
            return !(*this == other);
        }

        float Vector3::Dot(const Vector3& other) const {
            return x * other.x + y * other.y + z * other.z;
        }

        Vector3 Vector3::Cross(const Vector3& other) const {
            return Vector3(
                y * other.z - z * other.y,
                z * other.x - x * other.z,
                x * other.y - y * other.x
            );
        }

        float Vector3::Length() const {
            return std::sqrt(x * x + y * y + z * z);
        }

        float Vector3::LengthSquared() const {
            return x * x + y * y + z * z;
        }

        Vector3 Vector3::Normalized() const {
            float len = Length();
            if (len > 0.0f) {
                return *this / len;
            }
            return Vector3::Zero();
        }

        void Vector3::Normalize() {
            float len = Length();
            if (len > 0.0f) {
                x /= len;
                y /= len;
                z /= len;
            }
        }

        Vector3 Vector3::Zero() {
            return Vector3(0.0f, 0.0f, 0.0f);
        }

        Vector3 Vector3::One() {
            return Vector3(1.0f, 1.0f, 1.0f);
        }

        Vector3 Vector3::Up() {
            return Vector3(0.0f, 1.0f, 0.0f);
        }

        Vector3 Vector3::Down() {
            return Vector3(0.0f, -1.0f, 0.0f);
        }

        Vector3 Vector3::Left() {
            return Vector3(-1.0f, 0.0f, 0.0f);
        }

        Vector3 Vector3::Right() {
            return Vector3(1.0f, 0.0f, 0.0f);
        }

        Vector3 Vector3::Forward() {
            return Vector3(0.0f, 0.0f, 1.0f);
        }

        Vector3 Vector3::Back() {
            return Vector3(0.0f, 0.0f, -1.0f);
        }

    }
}