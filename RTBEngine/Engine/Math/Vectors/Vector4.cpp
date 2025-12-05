#include "Vector4.h"

namespace RTBEngine {
    namespace Math {

        Vector4::Vector4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}

        Vector4::Vector4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

        Vector4::Vector4(float value) : x(value), y(value), z(value), w(value) {}

        Vector4 Vector4::operator+(const Vector4& other) const {
            return Vector4(x + other.x, y + other.y, z + other.z, w + other.w);
        }

        Vector4 Vector4::operator-(const Vector4& other) const {
            return Vector4(x - other.x, y - other.y, z - other.z, w - other.w);
        }

        Vector4 Vector4::operator*(float scalar) const {
            return Vector4(x * scalar, y * scalar, z * scalar, w * scalar);
        }

        Vector4 Vector4::operator/(float scalar) const {
            return Vector4(x / scalar, y / scalar, z / scalar, w / scalar);
        }

        Vector4& Vector4::operator+=(const Vector4& other) {
            x += other.x;
            y += other.y;
            z += other.z;
            w += other.w;
            return *this;
        }

        Vector4& Vector4::operator-=(const Vector4& other) {
            x -= other.x;
            y -= other.y;
            z -= other.z;
            w -= other.w;
            return *this;
        }

        Vector4& Vector4::operator*=(float scalar) {
            x *= scalar;
            y *= scalar;
            z *= scalar;
            w *= scalar;
            return *this;
        }

        Vector4& Vector4::operator/=(float scalar) {
            x /= scalar;
            y /= scalar;
            z /= scalar;
            w /= scalar;
            return *this;
        }

        Vector4 Vector4::operator-() const {
            return Vector4(-x, -y, -z, -w);
        }

        bool Vector4::operator==(const Vector4& other) const {
            return x == other.x && y == other.y && z == other.z && w == other.w;
        }

        bool Vector4::operator!=(const Vector4& other) const {
            return !(*this == other);
        }

        float Vector4::Dot(const Vector4& other) const {
            return x * other.x + y * other.y + z * other.z + w * other.w;
        }

        float Vector4::Length() const {
            return std::sqrt(x * x + y * y + z * z + w * w);
        }

        float Vector4::LengthSquared() const {
            return x * x + y * y + z * z + w * w;
        }

        Vector4 Vector4::Normalized() const {
            float len = Length();
            if (len > 0.0f) {
                return *this / len;
            }
            return Vector4::Zero();
        }

        void Vector4::Normalize() {
            float len = Length();
            if (len > 0.0f) {
                x /= len;
                y /= len;
                z /= len;
                w /= len;
            }
        }

        Vector4 Vector4::Zero() {
            return Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        }

        Vector4 Vector4::One() {
            return Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        }

    }
}