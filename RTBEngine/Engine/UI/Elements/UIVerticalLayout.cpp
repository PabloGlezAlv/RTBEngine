#include "UIVerticalLayout.h"

#include "../../Reflection/PropertyMacros.h"

namespace RTBEngine {
    namespace UI {

        using ThisClass = UIVerticalLayout;
        RTB_REGISTER_COMPONENT(UIVerticalLayout)
            RTB_PROPERTY(padding)
            RTB_PROPERTY(spacing)
        RTB_END_REGISTER(UIVerticalLayout)

        void UIVerticalLayout::ApplyLayout() const
        {
            ApplyAxisLayout(false);
        }

    }
}
