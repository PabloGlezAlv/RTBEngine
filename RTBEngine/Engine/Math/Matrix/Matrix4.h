#pragma once
#include "../Vectors/Vector3.h"
#include "../Vectors/Vector4.h"
#include <cmath>

namespace RTBEngine {
    namespace Math {

        class Matrix4 {
        public:
            float m[16];

            Matrix4();
            Matrix4(float diagonal);
            Matrix4(const float* values);

            static Matrix4 Identity();

            float& operator[](int index);
            const float& operator[](int index) const;

            Matrix4 operator*(const Matrix4& other) const;
            Vector4 operator*(const Vector4& vec) const;
            Matrix4& operator*=(const Matrix4& other);

            static Matrix4 Translate(const Vector3& translation);
            static Matrix4 Scale(const Vector3& scale);
            static Matrix4 RotateX(float angle);
            static Matrix4 RotateY(float angle);
            static Matrix4 RotateZ(float angle);
            static Matrix4 Rotate(float angle, const Vector3& axis);

            static Matrix4 LookAt(const Vector3& eye, const Vector3& center, const Vector3& up);
            static Matrix4 Perspective(float fov, float aspect, float near, float far);
            static Matrix4 Orthographic(float left, float right, float bottom, float top, float near, float far);

            Matrix4 Transpose() const;
            Matrix4 Inverse() const;
        };

    }
}