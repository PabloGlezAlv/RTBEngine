#include "Matrix4.h"
#include <cstring>

namespace RTBEngine {
    namespace Math {

        Matrix4::Matrix4() {
            memset(m, 0, 16 * sizeof(float));
        }

        Matrix4::Matrix4(float diagonal) {
            memset(m, 0, 16 * sizeof(float));
            m[0] = diagonal;
            m[5] = diagonal;
            m[10] = diagonal;
            m[15] = diagonal;
        }

        Matrix4::Matrix4(const float* values) {
            memcpy(m, values, 16 * sizeof(float));
        }

        Matrix4 Matrix4::Identity() {
            return Matrix4(1.0f);
        }

        float& Matrix4::operator[](int index) {
            return m[index];
        }

        const float& Matrix4::operator[](int index) const {
            return m[index];
        }

        Matrix4 Matrix4::operator*(const Matrix4& other) const {
            Matrix4 result;
            for (int row = 0; row < 4; row++) {
                for (int col = 0; col < 4; col++) {
                    float sum = 0.0f;
                    for (int i = 0; i < 4; i++) {
                        sum += m[row + i * 4] * other.m[i + col * 4];
                    }
                    result.m[row + col * 4] = sum;
                }
            }
            return result;
        }

        Vector4 Matrix4::operator*(const Vector4& vec) const {
            return Vector4(
                m[0] * vec.x + m[4] * vec.y + m[8] * vec.z + m[12] * vec.w,
                m[1] * vec.x + m[5] * vec.y + m[9] * vec.z + m[13] * vec.w,
                m[2] * vec.x + m[6] * vec.y + m[10] * vec.z + m[14] * vec.w,
                m[3] * vec.x + m[7] * vec.y + m[11] * vec.z + m[15] * vec.w
            );
        }

        Matrix4& Matrix4::operator*=(const Matrix4& other) {
            *this = *this * other;
            return *this;
        }

        Matrix4 Matrix4::Translate(const Vector3& translation) {
            Matrix4 result = Identity();
            result.m[12] = translation.x;
            result.m[13] = translation.y;
            result.m[14] = translation.z;
            return result;
        }

        Matrix4 Matrix4::Scale(const Vector3& scale) {
            Matrix4 result;
            result.m[0] = scale.x;
            result.m[5] = scale.y;
            result.m[10] = scale.z;
            result.m[15] = 1.0f;
            return result;
        }

        Matrix4 Matrix4::RotateX(float angle) {
            Matrix4 result = Identity();
            float c = cos(angle);
            float s = sin(angle);
            result.m[5] = c;
            result.m[6] = s;
            result.m[9] = -s;
            result.m[10] = c;
            return result;
        }

        Matrix4 Matrix4::RotateY(float angle) {
            Matrix4 result = Identity();
            float c = cos(angle);
            float s = sin(angle);
            result.m[0] = c;
            result.m[2] = -s;
            result.m[8] = s;
            result.m[10] = c;
            return result;
        }

        Matrix4 Matrix4::RotateZ(float angle) {
            Matrix4 result = Identity();
            float c = cos(angle);
            float s = sin(angle);
            result.m[0] = c;
            result.m[1] = s;
            result.m[4] = -s;
            result.m[5] = c;
            return result;
        }

        Matrix4 Matrix4::Rotate(float angle, const Vector3& axis) {
            Matrix4 result = Identity();
            float c = cos(angle);
            float s = sin(angle);
            float t = 1.0f - c;

            Vector3 normalized = axis.Normalized();
            float x = normalized.x;
            float y = normalized.y;
            float z = normalized.z;

            result.m[0] = t * x * x + c;
            result.m[1] = t * x * y + s * z;
            result.m[2] = t * x * z - s * y;

            result.m[4] = t * x * y - s * z;
            result.m[5] = t * y * y + c;
            result.m[6] = t * y * z + s * x;

            result.m[8] = t * x * z + s * y;
            result.m[9] = t * y * z - s * x;
            result.m[10] = t * z * z + c;

            return result;
        }

        Matrix4 Matrix4::LookAt(const Vector3& eye, const Vector3& center, const Vector3& up) {
            Vector3 f = (center - eye).Normalized();
            Vector3 s = f.Cross(up).Normalized();
            Vector3 u = s.Cross(f);

            Matrix4 result = Identity();
            result.m[0] = s.x;
            result.m[4] = s.y;
            result.m[8] = s.z;
            result.m[1] = u.x;
            result.m[5] = u.y;
            result.m[9] = u.z;
            result.m[2] = -f.x;
            result.m[6] = -f.y;
            result.m[10] = -f.z;
            result.m[12] = -s.Dot(eye);
            result.m[13] = -u.Dot(eye);
            result.m[14] = f.Dot(eye);

            return result;
        }

        Matrix4 Matrix4::Perspective(float fov, float aspect, float near, float far) {
            Matrix4 result;
            float tanHalfFov = tan(fov / 2.0f);

            result.m[0] = 1.0f / (aspect * tanHalfFov);
            result.m[5] = 1.0f / tanHalfFov;
            result.m[10] = -(far + near) / (far - near);
            result.m[11] = -1.0f;
            result.m[14] = -(2.0f * far * near) / (far - near);

            return result;
        }

        Matrix4 Matrix4::Orthographic(float left, float right, float bottom, float top, float near, float far) {
            Matrix4 result = Identity();

            result.m[0] = 2.0f / (right - left);
            result.m[5] = 2.0f / (top - bottom);
            result.m[10] = -2.0f / (far - near);
            result.m[12] = -(right + left) / (right - left);
            result.m[13] = -(top + bottom) / (top - bottom);
            result.m[14] = -(far + near) / (far - near);

            return result;
        }

        Matrix4 Matrix4::Transpose() const {
            Matrix4 result;
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    result.m[i * 4 + j] = m[j * 4 + i];
                }
            }
            return result;
        }

        Matrix4 Matrix4::Inverse() const {
            Matrix4 inv;
            float det;

            inv.m[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
                m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];

            inv.m[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
                m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];

            inv.m[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
                m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];

            inv.m[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
                m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

            inv.m[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
                m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];

            inv.m[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
                m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];

            inv.m[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
                m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];

            inv.m[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
                m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

            inv.m[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
                m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];

            inv.m[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
                m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];

            inv.m[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
                m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];

            inv.m[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
                m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

            inv.m[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
                m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];

            inv.m[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
                m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];

            inv.m[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
                m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];

            inv.m[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
                m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

            det = m[0] * inv.m[0] + m[1] * inv.m[4] + m[2] * inv.m[8] + m[3] * inv.m[12];

            if (det == 0)
                return Identity();

            det = 1.0f / det;

            for (int i = 0; i < 16; i++)
                inv.m[i] *= det;

            return inv;
        }

    }
}