#include "TypeInfo.h"
#include <algorithm>
#include "../ECS/Component.h"

namespace RTBEngine {
    namespace Reflection {

        TypeInfo::TypeInfo(const char* typeName, FactoryFunc /*factory*/)
            : typeName(typeName ? typeName : "")
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

        void* PropertyInfo::GetMutableData(ECS::Component* component) const {
            return component ? GetMutableData(component->GetActualObject()) : nullptr;
        }

        const void* PropertyInfo::GetData(const ECS::Component* component) const {
            return component ? GetData(component->GetActualObject()) : nullptr;
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

        void TypeInfo::AddPropertyPOD(const char* name, PropertyType type, size_t offset, size_t size, PropertyFlags flags) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.type = type;
            prop.offset = offset;
            prop.size = size;
            prop.flags = flags;
            properties.push_back(std::move(prop));
        }

        void TypeInfo::AddPropertyPODRange(const char* name, PropertyType type, size_t offset, size_t size, PropertyFlags flags, float rangeMin, float rangeMax) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.type = type;
            prop.offset = offset;
            prop.size = size;
            prop.flags = flags;
            prop.range = Range(rangeMin, rangeMax);
            properties.push_back(std::move(prop));
        }

        void TypeInfo::AddPropertyPODEnum(const char* name, size_t offset, size_t size, PropertyFlags flags, const char* const* enumNames, int enumCount) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.type = PropertyType::Enum;
            prop.offset = offset;
            prop.size = size;
            prop.flags = flags;
            for (int i = 0; i < enumCount; ++i)
                prop.enumNames.push_back(enumNames[i]);
            properties.push_back(std::move(prop));
        }

        void TypeInfo::AddPropertyPODTyped(const char* name, PropertyType type, size_t offset, size_t size, PropertyFlags flags, const char* extraTypeName) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.type = type;
            prop.offset = offset;
            prop.size = size;
            prop.flags = flags;
            prop.componentTypeName = extraTypeName ? extraTypeName : "";
            properties.push_back(std::move(prop));
        }

        TypeRegistry& TypeRegistry::GetInstance() {
            static TypeRegistry instance;
            return instance;
        }

        void TypeRegistry::RegisterType(const std::string& typeName, const TypeInfo& info) {
            types[typeName] = info;
        }

        void TypeRegistry::UnregisterType(const std::string& typeName) {
            types.erase(typeName);
        }

        const TypeInfo* TypeRegistry::GetTypeInfo(const std::string& typeName) const {
            auto it = types.find(typeName);
            if (it != types.end()) {
                return &it->second;
            }
            return nullptr;
        }

        const TypeInfo* TypeRegistry::GetTypeInfo(const char* typeName) const {
            return typeName ? GetTypeInfo(std::string(typeName)) : nullptr;
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

        void TypeRegistry::ForEachType(void(*callback)(const char* typeName, const TypeInfo* info, void* userData), void* userData) const {
            for (const auto& pair : types) {
                callback(pair.first.c_str(), &pair.second, userData);
            }
        }

        RTBEngine::ECS::Component* TypeRegistry::CreateComponent(const std::string& typeName) const {
            auto it = types.find(typeName);
            if (it != types.end()) {
                return it->second.Create();
            }
            return nullptr;
        }

    }
}
