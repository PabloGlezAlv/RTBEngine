#pragma once

#include "../RTBEngineAPI.h"
#include "../Math/Color.h"
#include "../Math/Vectors/Vector3.h"

namespace RTBEngine {
    namespace Rendering {

        struct Particle {
            Math::Vector3 position;
            Math::Vector3 velocity;
            Math::Color color;
            float size = 1.0f;
            float lifetime = 0.0f;
            float age = 0.0f;
            float animationOffset = 0.0f;

            bool IsAlive() const { return lifetime > 0.0f && age < lifetime; }
        };

        enum class ParticleEmitterShape {
            Point,
            Sphere,
            Cone,
            Box
        };

    }
}
