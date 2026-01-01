#pragma once
#include "TypeInfo.h"
#include "PropertyMacros.h"

namespace RTBEngine {
    namespace Math {
        class Vector2;
        class Vector3;
        class Vector4;
        class Quaternion;
    }

    namespace Reflection {

        //API for building TypeInfo
        template<typename T>
        class TypeInfoBuilder {
        public:
            explicit TypeInfoBuilder(const char* typeName) : info(typeName) {}

            template<typename PropType>
            TypeInfoBuilder& Property(const char* name, PropType T::* member, PropertyFlags flags = PropertyFlags::None) {
                PropertyInfo prop = MakePropertyInfo<PropType>(name, GetMemberOffset(member), flags);
                info.AddProperty(prop);
                return *this;
            }

            template<typename PropType>
            TypeInfoBuilder& PropertyWithRange(const char* name, PropType T::* member, float min, float max, PropertyFlags flags = PropertyFlags::None) {
                PropertyInfo prop = MakePropertyInfo<PropType>(name, GetMemberOffset(member), flags);
                prop.range = Range(min, max);
                info.AddProperty(prop);
                return *this;
            }

            template<typename PropType>
            TypeInfoBuilder& PropertyWithTooltip(const char* name, PropType T::* member, const char* tooltip, PropertyFlags flags = PropertyFlags::None) {
                PropertyInfo prop = MakePropertyInfo<PropType>(name, GetMemberOffset(member), flags);
                prop.tooltip = tooltip;
                info.AddProperty(prop);
                return *this;
            }

            template<typename EnumType>
            TypeInfoBuilder& PropertyEnum(const char* name, EnumType T::* member, std::initializer_list<const char*> enumNames, PropertyFlags flags = PropertyFlags::None) {
                PropertyInfo prop;
                prop.name = name;
                prop.displayName = name;
                prop.offset = GetMemberOffset(member);
                prop.size = sizeof(EnumType);
                prop.flags = flags;
                prop.type = PropertyType::Enum;
                for (const char* enumName : enumNames) {
                    prop.enumNames.push_back(enumName);
                }
                info.AddProperty(prop);
                return *this;
            }

            template<typename AssetType>
            TypeInfoBuilder& PropertyAsset(const char* name, AssetType* T::* member, const char* assetTypeName, PropertyFlags flags = PropertyFlags::None) {
                PropertyInfo prop;
                prop.name = name;
                prop.displayName = name;
                prop.offset = GetMemberOffset(member);
                prop.size = sizeof(AssetType*);
                prop.flags = flags;
                prop.type = PropertyType::AssetRef;
                prop.assetType = assetTypeName;
                info.AddProperty(prop);
                return *this;
            }

            TypeInfoBuilder& Category(const char* categoryName) {
                currentCategory = categoryName;
                return *this;
            }

            TypeInfo Build() { return std::move(info); }
            TypeInfo& GetInfo() { return info; }

        private:
            TypeInfo info;
            std::string currentCategory;

            template<typename PropType>
            static size_t GetMemberOffset(PropType T::* member) {
                return reinterpret_cast<size_t>(&(static_cast<T*>(nullptr)->*member));
            }
        };

        // Math type specializations
        template<>
        inline PropertyInfo MakePropertyInfo<Math::Vector2>(const char* name, size_t offset, PropertyFlags flags) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.offset = offset;
            prop.size = sizeof(float) * 2;
            prop.flags = flags;
            prop.type = PropertyType::Vector2;
            return prop;
        }

        template<>
        inline PropertyInfo MakePropertyInfo<Math::Vector3>(const char* name, size_t offset, PropertyFlags flags) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.offset = offset;
            prop.size = sizeof(float) * 3;
            prop.flags = flags;
            prop.type = PropertyType::Vector3;
            return prop;
        }

        template<>
        inline PropertyInfo MakePropertyInfo<Math::Vector4>(const char* name, size_t offset, PropertyFlags flags) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.offset = offset;
            prop.size = sizeof(float) * 4;
            prop.flags = flags;
            prop.type = PropertyType::Vector4;
            return prop;
        }

        template<>
        inline PropertyInfo MakePropertyInfo<Math::Quaternion>(const char* name, size_t offset, PropertyFlags flags) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.offset = offset;
            prop.size = sizeof(float) * 4;
            prop.flags = flags;
            prop.type = PropertyType::Quaternion;
            return prop;
        }

    } 
}