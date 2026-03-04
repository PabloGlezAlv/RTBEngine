#pragma once
#include "../RTBEngineAPI.h"
#include "Vectors/Vector4.h"

namespace RTBEngine {
    namespace Math {

        struct RTB_API Color {
            float r, g, b, a;

            Color() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
            Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
            explicit Color(float value) : r(value), g(value), b(value), a(1.0f) {}

            Color(const Vector4& v) : r(v.x), g(v.y), b(v.z), a(v.w) {}
            operator Vector4() const { return Vector4(r, g, b, a); }

            static Color White()   { return Color(1.0f, 1.0f, 1.0f, 1.0f); }
            static Color Black()   { return Color(0.0f, 0.0f, 0.0f, 1.0f); }
            static Color Red()     { return Color(1.0f, 0.0f, 0.0f, 1.0f); }
            static Color Green()   { return Color(0.0f, 1.0f, 0.0f, 1.0f); }
            static Color Blue()    { return Color(0.0f, 0.0f, 1.0f, 1.0f); }
            static Color Clear()   { return Color(0.0f, 0.0f, 0.0f, 0.0f); }
        };

    }
}
