#pragma once
#include "TypeInfo.h"
#include "../Math/Math.h"

// Usage in header:
//   class MyComponent : public Component {
//   public:
//       float speed = 5.0f;           // Public -> visible in Inspector
//   private:
//       RTB_SERIALIZE()
//       float maxHealth = 100.0f;     // Private + RTB_SERIALIZE -> visible
//       float internalTimer = 0.0f;   // Private without macro -> NOT visible
//       RTB_COMPONENT(MyComponent)
//   };
//
// Usage in cpp:
//   using ThisClass = MyComponent;
//   RTB_REGISTER_COMPONENT(MyComponent)
//       RTB_PROPERTY(speed)
//       RTB_PROPERTY_SERIALIZED(maxHealth)
//   RTB_END_REGISTER()

// Marks a private variable as visible in Inspector (semantic marker only)
#define RTB_SERIALIZE()

// Required at end of component class - generates GetTypeName() and GetTypeInfo()
#define RTB_COMPONENT(ClassName)                                                        \
public:                                                                                 \
    virtual const char* GetTypeName() const override { return #ClassName; }             \
    virtual const RTBEngine::Reflection::TypeInfo* GetTypeInfo() const override {       \
        return &ClassName::StaticTypeInfo();                                            \
    }                                                                                   \
    static const RTBEngine::Reflection::TypeInfo& StaticTypeInfo() {                    \
        return MutableTypeInfo();                                                       \
    }                                                                                   \
    static RTBEngine::Reflection::TypeInfo& MutableTypeInfo() {                         \
        static RTBEngine::Reflection::TypeInfo info(#ClassName, []() -> RTBEngine::ECS::Component* { return new ClassName(); }); \
        return info;                                                                    \
    }                                                                                   \
private:

// Starts property registration in cpp file
#define RTB_REGISTER_COMPONENT(ClassName)                                               \
    namespace {                                                                         \
        struct ClassName##_TypeRegistrar {                                              \
            ClassName##_TypeRegistrar() {                                               \
                RTBEngine::Reflection::TypeInfo& info = ClassName::MutableTypeInfo();   \
                (void)info;

// Registers a public property
#define RTB_PROPERTY(PropName)                                                          \
                info.AddProperty(                                                       \
                    RTBEngine::Reflection::MakePropertyInfo<decltype(std::declval<ThisClass>().PropName)>( \
                        #PropName,                                                      \
                        offsetof(ThisClass, PropName),                                  \
                        RTBEngine::Reflection::PropertyFlags::None                      \
                    )                                                                   \
                );

// Registers a private property marked with RTB_SERIALIZE
#define RTB_PROPERTY_SERIALIZED(PropName)                                               \
                info.AddProperty(                                                       \
                    RTBEngine::Reflection::MakePropertyInfo<decltype(std::declval<ThisClass>().PropName)>( \
                        #PropName,                                                      \
                        offsetof(ThisClass, PropName),                                  \
                        RTBEngine::Reflection::PropertyFlags::Serialize                 \
                    )                                                                   \
                );

// Registers a property with range for sliders
#define RTB_PROPERTY_RANGE(PropName, Min, Max)                                          \
                {                                                                       \
                    auto prop = RTBEngine::Reflection::MakePropertyInfo<decltype(std::declval<ThisClass>().PropName)>( \
                        #PropName,                                                      \
                        offsetof(ThisClass, PropName),                                  \
                        RTBEngine::Reflection::PropertyFlags::None                      \
                    );                                                                  \
                    prop.range = RTBEngine::Reflection::Range(Min, Max);                \
                    info.AddProperty(prop);                                             \
                }

// Registers an enum property
#define RTB_PROPERTY_ENUM(PropName, ...)                                                \
                {                                                                       \
                    auto prop = RTBEngine::Reflection::MakePropertyInfo<decltype(std::declval<ThisClass>().PropName)>( \
                        #PropName,                                                      \
                        offsetof(ThisClass, PropName),                                  \
                        RTBEngine::Reflection::PropertyFlags::None                      \
                    );                                                                  \
                    prop.type = RTBEngine::Reflection::PropertyType::Enum;              \
                    prop.enumNames = { __VA_ARGS__ };                                   \
                    info.AddProperty(prop);                                             \
                }

// Registers a color property
#define RTB_PROPERTY_COLOR(PropName)                                                    \
                {                                                                       \
                    auto prop = RTBEngine::Reflection::MakePropertyInfo<decltype(std::declval<ThisClass>().PropName)>( \
                        #PropName,                                                      \
                        offsetof(ThisClass, PropName),                                  \
                        RTBEngine::Reflection::PropertyFlags::None                      \
                    );                                                                  \
                    prop.type = RTBEngine::Reflection::PropertyType::Color;             \
                    info.AddProperty(prop);                                             \
                }

// Registers a property hidden from inspector
#define RTB_PROPERTY_HIDDEN(PropName)                                                   \
                info.AddProperty(                                                       \
                    RTBEngine::Reflection::MakePropertyInfo<decltype(std::declval<ThisClass>().PropName)>( \
                        #PropName,                                                      \
                        offsetof(ThisClass, PropName),                                  \
                        RTBEngine::Reflection::PropertyFlags::HideInInspector           \
                    )                                                                   \
                );

// Registers a read-only property
#define RTB_PROPERTY_READONLY(PropName)                                                 \
                info.AddProperty(                                                       \
                    RTBEngine::Reflection::MakePropertyInfo<decltype(std::declval<ThisClass>().PropName)>( \
                        #PropName,                                                      \
                        offsetof(ThisClass, PropName),                                  \
                        RTBEngine::Reflection::PropertyFlags::ReadOnly                  \
                    )                                                                   \
                );

// Registers a Texture* property
#define RTB_PROPERTY_TEXTURE(PropName)                                                  \
                {                                                                       \
                    RTBEngine::Reflection::PropertyInfo prop;                           \
                    prop.name = #PropName;                                              \
                    prop.displayName = #PropName;                                       \
                    prop.offset = offsetof(ThisClass, PropName);                        \
                    prop.size = sizeof(void*);                                          \
                    prop.type = RTBEngine::Reflection::PropertyType::TextureRef;        \
                    prop.flags = RTBEngine::Reflection::PropertyFlags::None;            \
                    info.AddProperty(prop);                                             \
                }

// Registers an AudioClip* property
#define RTB_PROPERTY_AUDIOCLIP(PropName)                                                \
                {                                                                       \
                    RTBEngine::Reflection::PropertyInfo prop;                           \
                    prop.name = #PropName;                                              \
                    prop.displayName = #PropName;                                       \
                    prop.offset = offsetof(ThisClass, PropName);                        \
                    prop.size = sizeof(void*);                                          \
                    prop.type = RTBEngine::Reflection::PropertyType::AudioClipRef;      \
                    prop.flags = RTBEngine::Reflection::PropertyFlags::None;            \
                    info.AddProperty(prop);                                             \
                }

// Registers a Mesh* property
#define RTB_PROPERTY_MESH(PropName)                                                     \
                {                                                                       \
                    RTBEngine::Reflection::PropertyInfo prop;                           \
                    prop.name = #PropName;                                              \
                    prop.displayName = #PropName;                                       \
                    prop.offset = offsetof(ThisClass, PropName);                        \
                    prop.size = sizeof(void*);                                          \
                    prop.type = RTBEngine::Reflection::PropertyType::MeshRef;           \
                    prop.flags = RTBEngine::Reflection::PropertyFlags::None;            \
                    info.AddProperty(prop);                                             \
                }

// Registers a Font* property
#define RTB_PROPERTY_FONT(PropName)                                                     \
                {                                                                       \
                    RTBEngine::Reflection::PropertyInfo prop;                           \
                    prop.name = #PropName;                                              \
                    prop.displayName = #PropName;                                       \
                    prop.offset = offsetof(ThisClass, PropName);                        \
                    prop.size = sizeof(void*);                                          \
                    prop.type = RTBEngine::Reflection::PropertyType::FontRef;           \
                    prop.flags = RTBEngine::Reflection::PropertyFlags::None;            \
                    info.AddProperty(prop);                                             \
                }

// Registers a GameObject* property
#define RTB_PROPERTY_GAMEOBJECT(PropName)                                               \
                {                                                                       \
                    RTBEngine::Reflection::PropertyInfo prop;                           \
                    prop.name = #PropName;                                              \
                    prop.displayName = #PropName;                                       \
                    prop.offset = offsetof(ThisClass, PropName);                        \
                    prop.size = sizeof(void*);                                          \
                    prop.type = RTBEngine::Reflection::PropertyType::GameObjectRef;     \
                    prop.flags = RTBEngine::Reflection::PropertyFlags::None;            \
                    info.AddProperty(prop);                                             \
                }

// Registers a Component* property with target type filtering
#define RTB_PROPERTY_COMPONENT(PropName, ComponentType)                                 \
                {                                                                       \
                    RTBEngine::Reflection::PropertyInfo prop;                           \
                    prop.name = #PropName;                                              \
                    prop.displayName = #PropName;                                       \
                    prop.offset = offsetof(ThisClass, PropName);                        \
                    prop.size = sizeof(void*);                                          \
                    prop.type = RTBEngine::Reflection::PropertyType::ComponentRef;      \
                    prop.componentTypeName = #ComponentType;                            \
                    prop.flags = RTBEngine::Reflection::PropertyFlags::None;            \
                    info.AddProperty(prop);                                             \
                }

// Ends property registration - pass ClassName again
#define RTB_END_REGISTER(ClassName)                                                     \
                RTBEngine::Reflection::TypeRegistry::GetInstance().RegisterType(        \
                    info.GetTypeName(), info);                                          \
            }                                                                           \
        };                                                                              \
        static ClassName##_TypeRegistrar _##ClassName##_registrar;                      \
    }

namespace RTBEngine {
    namespace Reflection {

        // Base template - unknown type
        template<typename T>
        PropertyInfo MakePropertyInfo(const char* name, size_t offset, PropertyFlags flags) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.offset = offset;
            prop.size = sizeof(T);
            prop.flags = flags;
            prop.type = PropertyType::Unknown;
            return prop;
        }

        // Specializations for primitive types
        template<>
        inline PropertyInfo MakePropertyInfo<bool>(const char* name, size_t offset, PropertyFlags flags) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.offset = offset;
            prop.size = sizeof(bool);
            prop.flags = flags;
            prop.type = PropertyType::Bool;
            return prop;
        }

        template<>
        inline PropertyInfo MakePropertyInfo<int>(const char* name, size_t offset, PropertyFlags flags) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.offset = offset;
            prop.size = sizeof(int);
            prop.flags = flags;
            prop.type = PropertyType::Int;
            return prop;
        }

        template<>
        inline PropertyInfo MakePropertyInfo<float>(const char* name, size_t offset, PropertyFlags flags) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.offset = offset;
            prop.size = sizeof(float);
            prop.flags = flags;
            prop.type = PropertyType::Float;
            return prop;
        }

        template<>
        inline PropertyInfo MakePropertyInfo<double>(const char* name, size_t offset, PropertyFlags flags) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.offset = offset;
            prop.size = sizeof(double);
            prop.flags = flags;
            prop.type = PropertyType::Double;
            return prop;
        }

        template<>
        inline PropertyInfo MakePropertyInfo<std::string>(const char* name, size_t offset, PropertyFlags flags) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.offset = offset;
            prop.size = sizeof(std::string);
            prop.flags = flags;
            prop.type = PropertyType::String;
            return prop;
        }

        template<>
        inline PropertyInfo MakePropertyInfo<RTBEngine::Math::Vector2>(const char* name, size_t offset, PropertyFlags flags) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.offset = offset;
            prop.size = sizeof(RTBEngine::Math::Vector2);
            prop.flags = flags;
            prop.type = PropertyType::Vector2;
            return prop;
        }

        template<>
        inline PropertyInfo MakePropertyInfo<RTBEngine::Math::Vector3>(const char* name, size_t offset, PropertyFlags flags) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.offset = offset;
            prop.size = sizeof(RTBEngine::Math::Vector3);
            prop.flags = flags;
            prop.type = PropertyType::Vector3;
            return prop;
        }

        template<>
        inline PropertyInfo MakePropertyInfo<RTBEngine::Math::Vector4>(const char* name, size_t offset, PropertyFlags flags) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.offset = offset;
            prop.size = sizeof(RTBEngine::Math::Vector4);
            prop.flags = flags;
            prop.type = PropertyType::Vector4;
            return prop;
        }

        template<>
        inline PropertyInfo MakePropertyInfo<RTBEngine::Math::Quaternion>(const char* name, size_t offset, PropertyFlags flags) {
            PropertyInfo prop;
            prop.name = name;
            prop.displayName = name;
            prop.offset = offset;
            prop.size = sizeof(RTBEngine::Math::Quaternion);
            prop.flags = flags;
            prop.type = PropertyType::Quaternion;
            return prop;
        }

    } 
}
