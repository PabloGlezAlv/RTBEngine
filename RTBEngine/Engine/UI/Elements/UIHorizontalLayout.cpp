#include "UIHorizontalLayout.h"

#include "../../Reflection/PropertyMacros.h"

namespace RTBEngine {
    namespace UI {

        using ThisClass = UIHorizontalLayout;
        RTB_REGISTER_COMPONENT(UIHorizontalLayout)
            RTB_PROPERTY(padding)
            RTB_PROPERTY(spacing)
        RTB_END_REGISTER(UIHorizontalLayout)

        void UIHorizontalLayout::ApplyLayout() const
        {
            ApplyAxisLayout(true);
        }

    }
}
