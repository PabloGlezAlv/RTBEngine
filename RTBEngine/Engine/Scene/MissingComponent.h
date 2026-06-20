#pragma once
#include "../RTBEngineAPI.h"
#include "Component.h"
#include <string>

namespace RTBEngine {
    namespace ECS {

        // Placeholder inserted by SceneLoader when a component type is not found.
        // Preserves the original type name so it can be displayed in the inspector.
#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API MissingComponent : public Component {
        public:
            MissingComponent();
            explicit MissingComponent(const std::string& missingTypeName);
            ~MissingComponent() override;

            MissingComponent(const MissingComponent&) = delete;
            MissingComponent& operator=(const MissingComponent&) = delete;

            virtual const char* GetTypeName() const override { return "MissingComponent"; }

            const std::string& GetMissingTypeName() const { return missingTypeName; }

        private:
            std::string missingTypeName;
        };
#pragma warning(pop)

    }
}
