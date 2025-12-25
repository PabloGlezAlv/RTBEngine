#pragma once
#include <string>
#include "../Math/Vectors/Vector3.h"

namespace RTBEngine {
    namespace Core {

        struct WindowConfig {
            std::string title = "RTBEngine Application";
            int width = 1280;
            int height = 720;
            bool fullscreen = false;
            bool resizable = true;
            bool vSync = true;
        };

        struct PhysicsConfig {
            float timeStep = 1.0f / 60.0f;
            Math::Vector3 gravity = Math::Vector3(0.0f, -9.81f, 0.0f);
        };

        struct RenderingConfig {
            float clearColorR = 0.1f;
            float clearColorG = 0.1f;
            float clearColorB = 0.1f;
        };

        struct ApplicationConfig {
            WindowConfig window;
            PhysicsConfig physics;
            RenderingConfig rendering;
            std::string initialScenePath;
        };

    }
}
