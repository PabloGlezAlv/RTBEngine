#include "Quaternion.h"

namespace RTBEngine {
    namespace Math {

        Quaternion::Quaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}

        Quaternion::Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

        Quaternion::Quaternion(const Vector3& axis, float angle) {
            float halfAngle = angle * 0.5f;
            float s = sin(halfAngle);
            Vector3 normalized = axis.Normalized();

            x = normalized.x * s;
            y = normalized.y * s;
            z = normalized.z * s;
            w = cos(halfAngle);
        }

        Quaternion Quaternion::Identity() {
            return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
        }

        Quaternion Quaternion::operator+(const Quaternion& other) const {
            return Quaternion(x + other.x, y + other.y, z + other.z, w + other.w);
        }

        Quaternion Quaternion::operator-(const Quaternion& other) const {
            return Quaternion(x - other.x, y - other.y, z - other.z, w - other.w);
        }

        Quaternion Quaternion::operator*(const Quaternion& other) const {
            return Quaternion(
                w * other.x + x * other.w + y * other.z - z * other.y,
                w * other.y + y * other.w + z * other.x - x * other.z,
                w * other.z + z * other.w + x * other.y - y * other.x,
                w * other.w - x * other.x - y * other.y - z * other.z
            );
        }
        Vector3 Quaternion::operator*(const Vector3& v) const {
            
            Vector3 qv(x, y, z);
            Vector3 uv = Vector3(
                qv.y * v.z - qv.z * v.y,
                qv.z * v.x - qv.x * v.z,
                qv.x * v.y - qv.y * v.x
            );
            Vector3 uuv = Vector3(
                qv.y * uv.z - qv.z * uv.y,
                qv.z * uv.x - qv.x * uv.z,
                qv.x * uv.y - qv.y * uv.x
            );
            return v + (uv * w + uuv) * 2.0f;
        }


        Quaternion Quaternion::operator*(float scalar) const {
            return Quaternion(x * scalar, y * scalar, z * scalar, w * scalar);
        }

        Quaternion& Quaternion::operator+=(const Quaternion& other) {
            x += other.x;
            y += other.y;
            z += other.z;
            w += other.w;
            return *this;
        }

        Quaternion& Quaternion::operator-=(const Quaternion& other) {
            x -= other.x;
            y -= other.y;
            z -= other.z;
            w -= other.w;
            return *this;
        }

        Quaternion& Quaternion::operator*=(const Quaternion& other) {
            *this = *this * other;
            return *this;
        }

        Quaternion& Quaternion::operator*=(float scalar) {
            x *= scalar;
            y *= scalar;
            z *= scalar;
            w *= scalar;
            return *this;
        }

        bool Quaternion::operator==(const Quaternion& other) const {
            return x == other.x && y == other.y && z == other.z && w == other.w;
        }

        bool Quaternion::operator!=(const Quaternion& other) const {
            return !(*this == other);
        }

        float Quaternion::Dot(const Quaternion& other) const {
            return x * other.x + y * other.y + z * other.z + w * other.w;
        }

        float Quaternion::Length() const {
            return std::sqrt(x * x + y * y + z * z + w * w);
        }

        float Quaternion::LengthSquared() const {
            return x * x + y * y + z * z + w * w;
        }

        Quaternion Quaternion::Normalized() const {
            float len = Length();
            if (len > 0.0f) {
                return *this * (1.0f / len);
            }
            return Identity();
        }

        void Quaternion::Normalize() {
            float len = Length();
            if (len > 0.0f) {
                x /= len;
                y /= len;
                z /= len;
                w /= len;
            }
        }

        Quaternion Quaternion::Conjugate() const {
            return Quaternion(-x, -y, -z, w);
        }

        Quaternion Quaternion::Inverse() const {
            float lenSq = LengthSquared();
            if (lenSq > 0.0f) {
                Quaternion conj = Conjugate();
                return conj * (1.0f / lenSq);
            }
            return Identity();
        }

        Matrix4 Quaternion::ToMatrix() const {
            Matrix4 result = Matrix4::Identity();

            float xx = x * x;
            float yy = y * y;
            float zz = z * z;
            float xy = x * y;
            float xz = x * z;
            float yz = y * z;
            float wx = w * x;
            float wy = w * y;
            float wz = w * z;

            result.m[0] = 1.0f - 2.0f * (yy + zz);
            result.m[1] = 2.0f * (xy + wz);
            result.m[2] = 2.0f * (xz - wy);

            result.m[4] = 2.0f * (xy - wz);
            result.m[5] = 1.0f - 2.0f * (xx + zz);
            result.m[6] = 2.0f * (yz + wx);

            result.m[8] = 2.0f * (xz + wy);
            result.m[9] = 2.0f * (yz - wx);
            result.m[10] = 1.0f - 2.0f * (xx + yy);

            return result;
        }

        namespace {
            float WrapRadians(float radians)
            {
                constexpr float kTwoPi = 6.283185307179586f;
                float wrapped = std::fmod(radians + 3.14159265358979323846f, kTwoPi);
                if (wrapped < 0.0f) {
                    wrapped += kTwoPi;
                }
                return wrapped - 3.14159265358979323846f;
            }
        }

        Vector3 Quaternion::ToEulerAngles() const {
            Vector3 euler;

            // YXZ rotation order - inverse of FromEulerAngles
            float r21 = 2.0f * (y * z + w * x);  // sin(pitch)
            float r22 = 1.0f - 2.0f * (x * x + y * y);  // cos(pitch) * cos(yaw)
            float r20 = 2.0f * (x * z - w * y);  // -cos(pitch) * sin(yaw)
            float r01 = 2.0f * (x * y - w * z);  // cos(pitch) * sin(roll)
            float r11 = 1.0f - 2.0f * (x * x + z * z);  // cos(pitch) * cos(roll)

            // Pitch (rotation around X axis)
            if (std::abs(r21) >= 0.9999f) {
                // Gimbal lock case
                euler.x = std::copysign(3.14159265358979323846f / 2.0f, r21);
                euler.y = -std::atan2(-r20, r22);
                euler.z = 0.0f;
            } else {
                euler.x = std::asin(r21);
                euler.y = -std::atan2(-r20, r22);
                euler.z = std::atan2(-r01, r11);
            }

            euler.x = WrapRadians(euler.x);
            euler.y = WrapRadians(euler.y);
            euler.z = WrapRadians(euler.z);

            return euler;
        }

        Quaternion Quaternion::FromEulerAngles(float pitch, float yaw, float roll) {
            // Modified YXZ convention with forward = +Z at yaw=0, pitch=0
            // pitch = rotation around X axis (look up/down)
            // yaw = rotation around Y axis (turn left/right)
            // roll = rotation around Z axis (tilt)
            // NOTE: Yaw is negated to make default forward point toward +Z

            float hp = pitch * 0.5f;   // half pitch
            float hy = -yaw * 0.5f;    // half yaw (NEGATED for +Z forward convention)
            float hr = roll * 0.5f;    // half roll

            float cp = cos(hp);
            float sp = sin(hp);
            float cy = cos(hy);
            float sy = sin(hy);
            float cr = cos(hr);
            float sr = sin(hr);

            // YXZ order: Ry * Rx * Rz
            Quaternion q;
            q.w = cy * cp * cr + sy * sp * sr;
            q.x = cy * sp * cr + sy * cp * sr;
            q.y = sy * cp * cr - cy * sp * sr;
            q.z = cy * cp * sr - sy * sp * cr;

            return q;
        }

        Quaternion Quaternion::FromEulerAngles(const Vector3& euler) {
            return FromEulerAngles(euler.x, euler.y, euler.z);
        }

        Quaternion Quaternion::FromMatrix(const Matrix4& mat) {
            // Shepperd method: extract quaternion from upper-left 3x3 rotation matrix
            // Column-major layout: m[col*4 + row]
            float m00 = mat.m[0];  float m01 = mat.m[4];  float m02 = mat.m[8];
            float m10 = mat.m[1];  float m11 = mat.m[5];  float m12 = mat.m[9];
            float m20 = mat.m[2];  float m21 = mat.m[6];  float m22 = mat.m[10];

            float trace = m00 + m11 + m22;
            Quaternion q;

            if (trace > 0.0f) {
                float s = std::sqrt(trace + 1.0f) * 2.0f;
                q.w = 0.25f * s;
                q.x = (m21 - m12) / s;
                q.y = (m02 - m20) / s;
                q.z = (m10 - m01) / s;
            }
            else if (m00 > m11 && m00 > m22) {
                float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
                q.w = (m21 - m12) / s;
                q.x = 0.25f * s;
                q.y = (m01 + m10) / s;
                q.z = (m02 + m20) / s;
            }
            else if (m11 > m22) {
                float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
                q.w = (m02 - m20) / s;
                q.x = (m01 + m10) / s;
                q.y = 0.25f * s;
                q.z = (m12 + m21) / s;
            }
            else {
                float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
                q.w = (m10 - m01) / s;
                q.x = (m02 + m20) / s;
                q.y = (m12 + m21) / s;
                q.z = 0.25f * s;
            }

            return q.Normalized();
        }

        Quaternion Quaternion::Slerp(const Quaternion& a, const Quaternion& b, float t) {
            Quaternion qb = b;
            float cosHalfTheta = a.Dot(b);

            if (cosHalfTheta < 0.0f) {
                qb = b * -1.0f;
                cosHalfTheta = -cosHalfTheta;
            }

            if (cosHalfTheta >= 1.0f) {
                return a;
            }

            float halfTheta = std::acos(cosHalfTheta);
            float sinHalfTheta = std::sqrt(1.0f - cosHalfTheta * cosHalfTheta);

            if (std::abs(sinHalfTheta) < 0.001f) {
                return Quaternion(
                    a.x * 0.5f + qb.x * 0.5f,
                    a.y * 0.5f + qb.y * 0.5f,
                    a.z * 0.5f + qb.z * 0.5f,
                    a.w * 0.5f + qb.w * 0.5f
                );
            }

            float ratioA = std::sin((1.0f - t) * halfTheta) / sinHalfTheta;
            float ratioB = std::sin(t * halfTheta) / sinHalfTheta;

            return Quaternion(
                a.x * ratioA + qb.x * ratioB,
                a.y * ratioA + qb.y * ratioB,
                a.z * ratioA + qb.z * ratioB,
                a.w * ratioA + qb.w * ratioB
            );
        }

        Quaternion Quaternion::Lerp(const Quaternion& a, const Quaternion& b, float t) {
            return Quaternion(
                a.x + t * (b.x - a.x),
                a.y + t * (b.y - a.y),
                a.z + t * (b.z - a.z),
                a.w + t * (b.w - a.w)
            ).Normalized();
        }

    }
}