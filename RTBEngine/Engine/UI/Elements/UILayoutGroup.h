#pragma once

#include "../../Scene/Component.h"
#include "../../Math/Vectors/Vector2.h"
#include "../../RTBEngineAPI.h"

namespace RTBEngine {
    namespace UI {

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API UILayoutGroup : public Scene::Component {
        public:
            UILayoutGroup() = default;
            ~UILayoutGroup() override = default;

            RTBEngine::Math::Vector2 padding = RTBEngine::Math::Vector2(8.0f, 8.0f);
            float spacing = 4.0f;

            virtual void ApplyLayout() const = 0;

        protected:
            void ApplyAxisLayout(bool horizontal) const;
        };
#pragma warning(pop)

    }
}
