#pragma once

#include "Vectors/Vector2.h"
#include "Vectors/Vector3.h"
#include "Vectors/Vector4.h"
#include "Matrix/Matrix4.h"
#include "Quaternions/Quaternion.h"
#include "Color.h"

namespace RTBEngine {
    namespace Math {

        inline float Lerp(float a, float b, float t)
        {
            return a + (b - a) * t;
        }

        inline Vector2 Lerp(const Vector2& a, const Vector2& b, float t)
        {
            return a + (b - a) * t;
        }

        inline Vector3 Lerp(const Vector3& a, const Vector3& b, float t)
        {
            return a + (b - a) * t;
        }

        inline Vector4 Lerp(const Vector4& a, const Vector4& b, float t)
        {
            return a + (b - a) * t;
        }

    }
}
