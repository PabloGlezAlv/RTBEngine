#pragma once

#include "UILayoutGroup.h"
#include "../../Reflection/PropertyMacros.h"

namespace RTBEngine {
    namespace UI {

        class RTB_API UIHorizontalLayout : public UILayoutGroup {
        public:
            void ApplyLayout() const override;

            RTB_COMPONENT(UIHorizontalLayout)
        };

    }
}
