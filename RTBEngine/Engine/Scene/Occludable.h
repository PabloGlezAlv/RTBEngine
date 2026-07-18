#pragma once

#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Reflection/PropertyMacros.h"

namespace RTBEngine {
    namespace Scene {

        class MeshRenderer;

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API Occludable : public Component {
        public:
            Occludable() = default;
            ~Occludable() override = default;

            bool occluderEnabled = true;
            float boundsPadding = 0.0f;

            float GetCurrentFadeAlpha() const { return currentFadeAlpha; }
            void SetCurrentFadeAlpha(float alpha) { currentFadeAlpha = alpha; }

            MeshRenderer* GetMeshRenderer() const;

            RTB_COMPONENT(Occludable)

        private:
            float currentFadeAlpha = 1.0f;
        };
#pragma warning(pop)

    }
}
