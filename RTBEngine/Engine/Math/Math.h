#pragma once

#include "Vectors/Vector2.h"
#include "Vectors/Vector3.h"
#include "Vectors/Vector4.h"
#include "Matrix/Matrix4.h"
#include "Quaternions/Quaternion.h"
#include "Color.h"

namespace RTBEngine {
    namespace Math {

        inline float Clamp(float value, float minValue, float maxValue)
        {
            if (value < minValue) {
                return minValue;
            }
            if (value > maxValue) {
                return maxValue;
            }
            return value;
        }

        inline float Clamp01(float value)
        {
            return Clamp(value, 0.0f, 1.0f);
        }

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

        inline float EaseInCubic(float t)
        {
            t = Clamp01(t);
            return t * t * t;
        }

        inline float EaseOutCubic(float t)
        {
            t = 1.0f - Clamp01(t);
            return 1.0f - (t * t * t);
        }

        inline float EaseInOutCubic(float t)
        {
            t = Clamp01(t);
            if (t < 0.5f) {
                return 4.0f * t * t * t;
            }

            const float inverse = -2.0f * t + 2.0f;
            return 1.0f - ((inverse * inverse * inverse) * 0.5f);
        }

    }
}
