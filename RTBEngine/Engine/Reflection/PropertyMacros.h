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
    virtual void* GetActualObject() override { return this; }                           \
    virtual const void* GetActualObject() const override { return this; }               \
    virtual const RTBEngine::Reflection::TypeInfo* GetTypeInfo() const override {       \
        return &ClassName::StaticTypeInfo();                                            \
    }                                                                                   \
    static const RTBEngine::Reflection::TypeInfo& StaticTypeInfo() {                    \
        return MutableTypeInfo();                                                       \
    }                                                                                   \
    static RTBEngine::Reflection::TypeInfo& MutableTypeInfo() {                         \
        static RTBEngine::Reflection::TypeInfo info(#ClassName);                        \
        static bool registrarSet = false;                                               \
        if (!registrarSet) {                                                            \
            info.SetFactory([](void*) -> RTBEngine::ECS::Component* { return new ClassName(); }, nullptr); \
            info.SetDestroyer([](RTBEngine::ECS::Component* c, void*) { delete static_cast<ClassName*>(c); }, nullptr); \
            registrarSet = true;                                                        \
        }                                                                               \
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
                info.AddPropertyPOD(                                                    \
                    #PropName,                                                          \
                    RTBEngine::Reflection::DeducePropertyType<decltype(std::declval<ThisClass>().PropName)>(), \
                    offsetof(ThisClass, PropName),                                      \
                    sizeof(std::declval<ThisClass>().PropName),                          \
                    RTBEngine::Reflection::PropertyFlags::None                           \
                );

// Registers a private property marked with RTB_SERIALIZE
#define RTB_PROPERTY_SERIALIZED(PropName)                                               \
                info.AddPropertyPOD(                                                    \
                    #PropName,                                                          \
                    RTBEngine::Reflection::DeducePropertyType<decltype(std::declval<ThisClass>().PropName)>(), \
                    offsetof(ThisClass, PropName),                                      \
                    sizeof(std::declval<ThisClass>().PropName),                          \
                    RTBEngine::Reflection::PropertyFlags::Serialize                      \
                );

// Registers a property with range for sliders
#define RTB_PROPERTY_RANGE(PropName, Min, Max)                                          \
                info.AddPropertyPODRange(                                               \
                    #PropName,                                                          \
                    RTBEngine::Reflection::DeducePropertyType<decltype(std::declval<ThisClass>().PropName)>(), \
                    offsetof(ThisClass, PropName),                                      \
                    sizeof(std::declval<ThisClass>().PropName),                          \
                    RTBEngine::Reflection::PropertyFlags::None,                          \
                    static_cast<float>(Min),                                             \
                    static_cast<float>(Max)                                              \
                );

// Registers an enum property
#define RTB_PROPERTY_ENUM(PropName, ...)                                                \
                {                                                                       \
                    const char* _rtb_enum_names[] = { __VA_ARGS__ };                    \
                    info.AddPropertyPODEnum(                                             \
                        #PropName,                                                      \
                        offsetof(ThisClass, PropName),                                  \
                        sizeof(std::declval<ThisClass>().PropName),                      \
                        RTBEngine::Reflection::PropertyFlags::None,                      \
                        _rtb_enum_names,                                                 \
                        static_cast<int>(sizeof(_rtb_enum_names) / sizeof(_rtb_enum_names[0])) \
                    );                                                                  \
                }

// Registers a color property
#define RTB_PROPERTY_COLOR(PropName)                                                    \
                info.AddPropertyPOD(                                                    \
                    #PropName,                                                          \
                    RTBEngine::Reflection::PropertyType::Color,                          \
                    offsetof(ThisClass, PropName),                                      \
                    sizeof(std::declval<ThisClass>().PropName),                          \
                    RTBEngine::Reflection::PropertyFlags::None                           \
                );

// Registers a property hidden from inspector
#define RTB_PROPERTY_HIDDEN(PropName)                                                   \
                info.AddPropertyPOD(                                                    \
                    #PropName,                                                          \
                    RTBEngine::Reflection::DeducePropertyType<decltype(std::declval<ThisClass>().PropName)>(), \
                    offsetof(ThisClass, PropName),                                      \
                    sizeof(std::declval<ThisClass>().PropName),                          \
                    RTBEngine::Reflection::PropertyFlags::HideInInspector                \
                );

// Registers a read-only property
#define RTB_PROPERTY_READONLY(PropName)                                                 \
                info.AddPropertyPOD(                                                    \
                    #PropName,                                                          \
                    RTBEngine::Reflection::DeducePropertyType<decltype(std::declval<ThisClass>().PropName)>(), \
                    offsetof(ThisClass, PropName),                                      \
                    sizeof(std::declval<ThisClass>().PropName),                          \
                    RTBEngine::Reflection::PropertyFlags::ReadOnly                       \
                );

// Registers a Texture* property
#define RTB_PROPERTY_TEXTURE(PropName)                                                  \
                info.AddPropertyPOD(                                                    \
                    #PropName,                                                          \
                    RTBEngine::Reflection::PropertyType::TextureRef,                     \
                    offsetof(ThisClass, PropName),                                      \
                    sizeof(void*),                                                       \
                    RTBEngine::Reflection::PropertyFlags::None                           \
                );

// Registers an AudioClip* property
#define RTB_PROPERTY_AUDIOCLIP(PropName)                                                \
                info.AddPropertyPOD(                                                    \
                    #PropName,                                                          \
                    RTBEngine::Reflection::PropertyType::AudioClipRef,                   \
                    offsetof(ThisClass, PropName),                                      \
                    sizeof(void*),                                                       \
                    RTBEngine::Reflection::PropertyFlags::None                           \
                );

// Registers a Mesh* property
#define RTB_PROPERTY_MESH(PropName)                                                     \
                info.AddPropertyPOD(                                                    \
                    #PropName,                                                          \
                    RTBEngine::Reflection::PropertyType::MeshRef,                        \
                    offsetof(ThisClass, PropName),                                      \
                    sizeof(void*),                                                       \
                    RTBEngine::Reflection::PropertyFlags::None                           \
                );

// Registers a Font* property
#define RTB_PROPERTY_FONT(PropName)                                                     \
                info.AddPropertyPOD(                                                    \
                    #PropName,                                                          \
                    RTBEngine::Reflection::PropertyType::FontRef,                        \
                    offsetof(ThisClass, PropName),                                      \
                    sizeof(void*),                                                       \
                    RTBEngine::Reflection::PropertyFlags::None                           \
                );

// Registers a GameObject* property
#define RTB_PROPERTY_GAMEOBJECT(PropName)                                               \
                info.AddPropertyPOD(                                                    \
                    #PropName,                                                          \
                    RTBEngine::Reflection::PropertyType::GameObjectRef,                  \
                    offsetof(ThisClass, PropName),                                      \
                    sizeof(void*),                                                       \
                    RTBEngine::Reflection::PropertyFlags::None                           \
                );

// Registers a Component* property with target type filtering
#define RTB_PROPERTY_COMPONENT(PropName, ComponentType)                                 \
                info.AddPropertyPODTyped(                                               \
                    #PropName,                                                          \
                    RTBEngine::Reflection::PropertyType::ComponentRef,                   \
                    offsetof(ThisClass, PropName),                                      \
                    sizeof(void*),                                                       \
                    RTBEngine::Reflection::PropertyFlags::None,                          \
                    #ComponentType                                                       \
                );

// Forward declaration visible to all script .cpp files when building GameScripts.dll.
#ifdef GAMESCRIPTS_EXPORTS
extern "C" void RTBScripts_RegisterLocalType(const char* typeName, const RTBEngine::Reflection::TypeInfo* info);
#endif

// Ends property registration - pass ClassName again
// When compiling GameScripts.dll, register into the local POD list (no STL across boundary).
// Otherwise register directly into the engine TypeRegistry.
#ifdef GAMESCRIPTS_EXPORTS
#define RTB_END_REGISTER(ClassName)                                                                     RTBScripts_RegisterLocalType(#ClassName, &ClassName::StaticTypeInfo());             }                                                                                   };                                                                                      static ClassName##_TypeRegistrar _##ClassName##_registrar;                          }
#else
#define RTB_END_REGISTER(ClassName)                                                                     RTBEngine::Reflection::TypeRegistry::GetInstance().RegisterType(                             #ClassName, ClassName::MutableTypeInfo());                                        }                                                                                   };                                                                                      static ClassName##_TypeRegistrar _##ClassName##_registrar;                          }
#endif

namespace RTBEngine {
    namespace Reflection {

        // Compile-time property type deduction — returns a plain enum, no code generation in caller
        template<typename T> constexpr PropertyType DeducePropertyType() { return PropertyType::Unknown; }
        template<> constexpr PropertyType DeducePropertyType<bool>() { return PropertyType::Bool; }
        template<> constexpr PropertyType DeducePropertyType<int>() { return PropertyType::Int; }
        template<> constexpr PropertyType DeducePropertyType<float>() { return PropertyType::Float; }
        template<> constexpr PropertyType DeducePropertyType<double>() { return PropertyType::Double; }
        template<> constexpr PropertyType DeducePropertyType<std::string>() { return PropertyType::String; }
        template<> constexpr PropertyType DeducePropertyType<RTBEngine::Math::Vector2>() { return PropertyType::Vector2; }
        template<> constexpr PropertyType DeducePropertyType<RTBEngine::Math::Vector3>() { return PropertyType::Vector3; }
        template<> constexpr PropertyType DeducePropertyType<RTBEngine::Math::Vector4>() { return PropertyType::Vector4; }
        template<> constexpr PropertyType DeducePropertyType<RTBEngine::Math::Quaternion>() { return PropertyType::Quaternion; }

    }
}
