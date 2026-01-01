#include "TypeInfo.h"
#include <algorithm>

namespace RTBEngine {
    namespace Reflection {

        TypeInfo::TypeInfo(const std::string& typeName)
            : typeName(typeName)
        {
        }

        const PropertyInfo* TypeInfo::GetProperty(const std::string& name) const {
            for (const auto& prop : properties) {
                if (prop.name == name) {
                    return &prop;
                }
            }
            return nullptr;
        }

        std::vector<const PropertyInfo*> TypeInfo::GetInspectorProperties() const {
            std::vector<const PropertyInfo*> result;
            for (const auto& prop : properties) {
                if (prop.IsVisibleInInspector()) {
                    result.push_back(&prop);
                }
            }
            return result;
        }

        std::vector<const PropertyInfo*> TypeInfo::GetSerializableProperties() const {
            std::vector<const PropertyInfo*> result;
            for (const auto& prop : properties) {
                if (prop.IsSerializable()) {
                    result.push_back(&prop);
                }
            }
            return result;
        }

        void TypeInfo::AddProperty(const PropertyInfo& prop) {
            properties.push_back(prop);
        }

        TypeRegistry& TypeRegistry::GetInstance() {
            static TypeRegistry instance;
            return instance;
        }

        void TypeRegistry::RegisterType(const std::string& typeName, const TypeInfo& info) {
            types[typeName] = info;
        }

        const TypeInfo* TypeRegistry::GetTypeInfo(const std::string& typeName) const {
            auto it = types.find(typeName);
            if (it != types.end()) {
                return &it->second;
            }
            return nullptr;
        }

        bool TypeRegistry::HasType(const std::string& typeName) const {
            return types.find(typeName) != types.end();
        }

        std::vector<std::string> TypeRegistry::GetRegisteredTypes() const {
            std::vector<std::string> result;
            result.reserve(types.size());
            for (const auto& pair : types) {
                result.push_back(pair.first);
            }
            return result;
        }

    }
}
