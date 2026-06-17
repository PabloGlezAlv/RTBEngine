#pragma once
#include "TypeInfo.h"
#include "../Math/Math.h"
#include "../Scripting/ScriptBridgeABI.h"
#include <utility>

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

#define RTB_MEMBER_OFFSET(PropName)                                                    \
    RTBEngine::Reflection::GetMemberOffset<RTBCurrentClass>(&ThisClass::PropName)

#ifdef GAMESCRIPTS_EXPORTS
#define RTB__BRIDGE_PROP(NameLiteral, TypeValue, OffsetValue, SizeValue, FlagsValue, RangeMinValue, RangeMaxValue, HasRangeValue, AssetTypeNameValue, ComponentTypeNameValue) \
                do {                                                                    \
                    RTBPropertyDesc _rtb_prop_desc{};                                   \
                    _rtb_prop_desc.name = (NameLiteral);                                \
                    _rtb_prop_desc.displayName = (NameLiteral);                         \
                    _rtb_prop_desc.type = static_cast<int>(TypeValue);                  \
                    _rtb_prop_desc.offset = static_cast<size_t>(OffsetValue);           \
                    _rtb_prop_desc.size = static_cast<size_t>(SizeValue);               \
                    _rtb_prop_desc.flags = static_cast<int>(FlagsValue);                \
                    _rtb_prop_desc.rangeMin = static_cast<float>(RangeMinValue);        \
                    _rtb_prop_desc.rangeMax = static_cast<float>(RangeMaxValue);        \
                    _rtb_prop_desc.hasRange = static_cast<int>(HasRangeValue);          \
                    _rtb_prop_desc.assetTypeName = (AssetTypeNameValue);                \
                    _rtb_prop_desc.componentTypeName = (ComponentTypeNameValue);        \
                    RTBScripts_RegisterLocalProperty(RTBCurrentTypeName, &_rtb_prop_desc); \
                } while (false)
#else
#define RTB__BRIDGE_PROP(...) do { } while (false)
#endif

// Required at end of component class - generates GetTypeName() and GetTypeInfo()
#ifdef GAMESCRIPTS_EXPORTS
#define RTB_COMPONENT(ClassName)                                                        \
public:                                                                                 \
    virtual const char* GetTypeName() const override { return #ClassName; }             \
    static constexpr const char* StaticComponentTypeName() { return #ClassName; }       \
    virtual void* GetActualObject() override { return this; }                           \
    virtual const void* GetActualObject() const override { return this; }               \
    virtual const RTBEngine::Reflection::TypeInfo* GetTypeInfo() const override {       \
        return RTBEngine::Reflection::TypeRegistry::GetInstance().GetTypeInfo(#ClassName); \
    }                                                                                   \
private:
#else
#define RTB_COMPONENT(ClassName)                                                        \
public:                                                                                 \
    virtual const char* GetTypeName() const override { return #ClassName; }             \
    static constexpr const char* StaticComponentTypeName() { return #ClassName; }       \
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
#endif

// Starts property registration in cpp file
#ifdef GAMESCRIPTS_EXPORTS
#define RTB_REGISTER_COMPONENT(ClassName)                                               \
    struct ClassName##_TypeRegistrar {                                                  \
        ClassName##_TypeRegistrar() {                                                   \
            using RTBCurrentClass = ClassName;                                          \
            constexpr const char* RTBCurrentTypeName = #ClassName;
#else
#define RTB_REGISTER_COMPONENT(ClassName)                                               \
    struct ClassName##_TypeRegistrar {                                                  \
        ClassName##_TypeRegistrar() {                                                   \
            RTBEngine::Reflection::TypeInfo& info = ClassName::MutableTypeInfo();       \
            using RTBCurrentClass = ClassName;                                          \
            constexpr const char* RTBCurrentTypeName = #ClassName;                      \
            (void)info;
#endif

// ─── Internal helpers ───

// RTB__PROP_POD: POD property + bridge
#ifdef GAMESCRIPTS_EXPORTS
#define RTB__PROP_POD(PropName, TypeExpr, SizeExpr, FlagsExpr)                          \
                RTB__BRIDGE_PROP(                                                       \
                    #PropName, TypeExpr, RTB_MEMBER_OFFSET(PropName),                   \
                    SizeExpr, FlagsExpr, 0.0f, 0.0f, 0, nullptr, nullptr               \
                );
#else
#define RTB__PROP_POD(PropName, TypeExpr, SizeExpr, FlagsExpr)                          \
                info.AddPropertyPOD(                                                    \
                    #PropName, TypeExpr, RTB_MEMBER_OFFSET(PropName),                   \
                    SizeExpr, FlagsExpr                                                 \
                );                                                                      \
                RTB__BRIDGE_PROP(                                                       \
                    #PropName, TypeExpr, RTB_MEMBER_OFFSET(PropName),                   \
                    SizeExpr, FlagsExpr, 0.0f, 0.0f, 0, nullptr, nullptr               \
                );
#endif

// RTB__PROP_REF: pointer-sized reference property
#define RTB__PROP_REF(PropName, TypeVal)                                                \
                RTB__PROP_POD(PropName, TypeVal, sizeof(void*),                         \
                    RTBEngine::Reflection::PropertyFlags::None)

// ─── Auto-deduced type & size ───
#define RTB__AUTO_TYPE(PropName)                                                        \
    RTBEngine::Reflection::DeducePropertyType<decltype(std::declval<ThisClass>().PropName)>()
#define RTB__AUTO_SIZE(PropName) sizeof(std::declval<ThisClass>().PropName)

// Registers a Serialize+HideInInspector field on an inline sub-object
#ifdef GAMESCRIPTS_EXPORTS
#define RTB_PROPERTY_NESTED_HIDDEN(OuterProp, InnerType, InnerProp)                     \
                {                                                                       \
                    InnerType _rtb_probe;                                                \
                    const size_t _rtb_inner_off =                                       \
                        reinterpret_cast<const char*>(&_rtb_probe.InnerProp)             \
                        - reinterpret_cast<const char*>(&_rtb_probe);                    \
                    const size_t _rtb_off =                                                      \
                        RTBEngine::Reflection::GetMemberOffset<RTBCurrentClass>(                 \
                            &RTBCurrentClass::OuterProp) + _rtb_inner_off;                      \
                    RTB__BRIDGE_PROP(                                                    \
                        #InnerProp,                                                      \
                        RTBEngine::Reflection::DeducePropertyType<                       \
                            decltype(std::declval<InnerType>().InnerProp)>(),             \
                        _rtb_off,                                                        \
                        sizeof(std::declval<InnerType>().InnerProp),                     \
                        RTBEngine::Reflection::PropertyFlags::Serialize |                 \
                        RTBEngine::Reflection::PropertyFlags::HideInInspector,            \
                        0.0f, 0.0f, 0, nullptr, nullptr                                 \
                    );                                                                   \
                }
#else
#define RTB_PROPERTY_NESTED_HIDDEN(OuterProp, InnerType, InnerProp)                     \
                {                                                                       \
                    InnerType _rtb_probe;                                                \
                    const size_t _rtb_inner_off =                                       \
                        reinterpret_cast<const char*>(&_rtb_probe.InnerProp)             \
                        - reinterpret_cast<const char*>(&_rtb_probe);                    \
                    const size_t _rtb_off =                                                      \
                        RTBEngine::Reflection::GetMemberOffset<RTBCurrentClass>(                 \
                            &RTBCurrentClass::OuterProp) + _rtb_inner_off;                      \
                    info.AddPropertyPOD(                                                 \
                        #InnerProp,                                                      \
                        RTBEngine::Reflection::DeducePropertyType<                       \
                            decltype(std::declval<InnerType>().InnerProp)>(),             \
                        _rtb_off,                                                        \
                        sizeof(std::declval<InnerType>().InnerProp),                     \
                        RTBEngine::Reflection::PropertyFlags::Serialize |                 \
                        RTBEngine::Reflection::PropertyFlags::HideInInspector             \
                    );                                                                   \
                    RTB__BRIDGE_PROP(                                                    \
                        #InnerProp,                                                      \
                        RTBEngine::Reflection::DeducePropertyType<                       \
                            decltype(std::declval<InnerType>().InnerProp)>(),             \
                        _rtb_off,                                                        \
                        sizeof(std::declval<InnerType>().InnerProp),                     \
                        RTBEngine::Reflection::PropertyFlags::Serialize |                 \
                        RTBEngine::Reflection::PropertyFlags::HideInInspector,            \
                        0.0f, 0.0f, 0, nullptr, nullptr                                 \
                    );                                                                   \
                }
#endif

// ─── Public property macros ─────────────────────────────────────────────────

// Registers a public property
#define RTB_PROPERTY(PropName)                                                          \
                RTB__PROP_POD(PropName, RTB__AUTO_TYPE(PropName),                       \
                    RTB__AUTO_SIZE(PropName),                                            \
                    RTBEngine::Reflection::PropertyFlags::None)

// Registers a private property marked with RTB_SERIALIZE
#define RTB_PROPERTY_SERIALIZED(PropName)                                               \
                RTB__PROP_POD(PropName, RTB__AUTO_TYPE(PropName),                       \
                    RTB__AUTO_SIZE(PropName),                                            \
                    RTBEngine::Reflection::PropertyFlags::Serialize)

#define RTB_PROPERTY_SERIALIZED_HIDDEN(PropName)                                        \
                RTB__PROP_POD(PropName, RTB__AUTO_TYPE(PropName),                       \
                    RTB__AUTO_SIZE(PropName),                                            \
                    RTBEngine::Reflection::PropertyFlags::Serialize | RTBEngine::Reflection::PropertyFlags::HideInInspector)

// Registers a property with range for sliders
#ifdef GAMESCRIPTS_EXPORTS
#define RTB_PROPERTY_RANGE(PropName, Min, Max)                                          \
                RTB__BRIDGE_PROP(                                                       \
                    #PropName, RTB__AUTO_TYPE(PropName),                                \
                    RTB_MEMBER_OFFSET(PropName), RTB__AUTO_SIZE(PropName),               \
                    RTBEngine::Reflection::PropertyFlags::None,                         \
                    static_cast<float>(Min), static_cast<float>(Max), 1, nullptr, nullptr \
                );
#else
#define RTB_PROPERTY_RANGE(PropName, Min, Max)                                          \
                info.AddPropertyPODRange(                                               \
                    #PropName, RTB__AUTO_TYPE(PropName),                                \
                    RTB_MEMBER_OFFSET(PropName), RTB__AUTO_SIZE(PropName),               \
                    RTBEngine::Reflection::PropertyFlags::None,                          \
                    static_cast<float>(Min), static_cast<float>(Max)                     \
                );                                                                      \
                RTB__BRIDGE_PROP(                                                       \
                    #PropName, RTB__AUTO_TYPE(PropName),                                \
                    RTB_MEMBER_OFFSET(PropName), RTB__AUTO_SIZE(PropName),               \
                    RTBEngine::Reflection::PropertyFlags::None,                         \
                    static_cast<float>(Min), static_cast<float>(Max), 1, nullptr, nullptr \
                );
#endif

// Registers an enum property
#ifdef GAMESCRIPTS_EXPORTS
#define RTB_PROPERTY_ENUM(PropName, ...)                                                \
                RTB__BRIDGE_PROP(                                                       \
                    #PropName, RTBEngine::Reflection::PropertyType::Enum,                \
                    RTB_MEMBER_OFFSET(PropName), RTB__AUTO_SIZE(PropName),               \
                    RTBEngine::Reflection::PropertyFlags::None,                         \
                    0.0f, 0.0f, 0, nullptr, nullptr                                     \
                );
#else
#define RTB_PROPERTY_ENUM(PropName, ...)                                                \
                {                                                                       \
                    const char* _rtb_enum_names[] = { __VA_ARGS__ };                    \
                    info.AddPropertyPODEnum(                                             \
                        #PropName, RTB_MEMBER_OFFSET(PropName),                         \
                        RTB__AUTO_SIZE(PropName),                                        \
                        RTBEngine::Reflection::PropertyFlags::None,                      \
                        _rtb_enum_names,                                                 \
                        static_cast<int>(sizeof(_rtb_enum_names) / sizeof(_rtb_enum_names[0])) \
                    );                                                                  \
                    RTB__BRIDGE_PROP(                                                   \
                        #PropName, RTBEngine::Reflection::PropertyType::Enum,            \
                        RTB_MEMBER_OFFSET(PropName), RTB__AUTO_SIZE(PropName),           \
                        RTBEngine::Reflection::PropertyFlags::None,                     \
                        0.0f, 0.0f, 0, nullptr, nullptr                                 \
                    );                                                                  \
                }
#endif

#ifdef GAMESCRIPTS_EXPORTS
#define RTB_PROPERTY_ASSET_PATH(PropName, AssetTypeName)                                \
                RTB__BRIDGE_PROP(                                                       \
                    #PropName, RTBEngine::Reflection::PropertyType::AssetRef,           \
                    RTB_MEMBER_OFFSET(PropName), RTB__AUTO_SIZE(PropName),              \
                    RTBEngine::Reflection::PropertyFlags::None,                         \
                    0.0f, 0.0f, 0, AssetTypeName, nullptr                               \
                );
#else
#define RTB_PROPERTY_ASSET_PATH(PropName, AssetTypeName)                                \
                info.AddPropertyPODTyped(                                               \
                    #PropName, RTBEngine::Reflection::PropertyType::AssetRef,           \
                    RTB_MEMBER_OFFSET(PropName), RTB__AUTO_SIZE(PropName),              \
                    RTBEngine::Reflection::PropertyFlags::None, AssetTypeName           \
                );                                                                      \
                RTB__BRIDGE_PROP(                                                       \
                    #PropName, RTBEngine::Reflection::PropertyType::AssetRef,           \
                    RTB_MEMBER_OFFSET(PropName), RTB__AUTO_SIZE(PropName),              \
                    RTBEngine::Reflection::PropertyFlags::None,                         \
                    0.0f, 0.0f, 0, AssetTypeName, nullptr                               \
                );
#endif

#define RTB_PROPERTY_FBX(PropName)                                                      \
                RTB_PROPERTY_ASSET_PATH(PropName, "fbx")

// Registers a color property
#define RTB_PROPERTY_COLOR(PropName)                                                    \
                RTB__PROP_POD(PropName, RTBEngine::Reflection::PropertyType::Color,     \
                    RTB__AUTO_SIZE(PropName),                                            \
                    RTBEngine::Reflection::PropertyFlags::None)

// Registers a property hidden from inspector
#define RTB_PROPERTY_HIDDEN(PropName)                                                   \
                RTB__PROP_POD(PropName, RTB__AUTO_TYPE(PropName),                       \
                    RTB__AUTO_SIZE(PropName),                                            \
                    RTBEngine::Reflection::PropertyFlags::HideInInspector)

// Registers a read-only property
#define RTB_PROPERTY_READONLY(PropName)                                                 \
                RTB__PROP_POD(PropName, RTB__AUTO_TYPE(PropName),                       \
                    RTB__AUTO_SIZE(PropName),                                            \
                    RTBEngine::Reflection::PropertyFlags::ReadOnly)

// Registers a Texture* property
#define RTB_PROPERTY_TEXTURE(PropName)                                                  \
                RTB__PROP_REF(PropName, RTBEngine::Reflection::PropertyType::TextureRef)

// Registers an AudioClip* property
#define RTB_PROPERTY_AUDIOCLIP(PropName)                                                \
                RTB__PROP_REF(PropName, RTBEngine::Reflection::PropertyType::AudioClipRef)

// Registers a Mesh* property
#define RTB_PROPERTY_MESH(PropName)                                                     \
                RTB__PROP_REF(PropName, RTBEngine::Reflection::PropertyType::MeshRef)

// Registers a Font* property
#define RTB_PROPERTY_FONT(PropName)                                                     \
                RTB__PROP_REF(PropName, RTBEngine::Reflection::PropertyType::FontRef)

// Registers a GameObject* property
#define RTB_PROPERTY_GAMEOBJECT(PropName)                                               \
                RTB__PROP_REF(PropName, RTBEngine::Reflection::PropertyType::GameObjectRef)

// Registers a Component* property with target type filtering
#ifdef GAMESCRIPTS_EXPORTS
#define RTB_PROPERTY_COMPONENT(PropName, ComponentType)                                 \
                RTB__BRIDGE_PROP(                                                       \
                    #PropName, RTBEngine::Reflection::PropertyType::ComponentRef,        \
                    RTB_MEMBER_OFFSET(PropName), sizeof(void*),                         \
                    RTBEngine::Reflection::PropertyFlags::None,                         \
                    0.0f, 0.0f, 0, nullptr, #ComponentType                              \
                );
#else
#define RTB_PROPERTY_COMPONENT(PropName, ComponentType)                                 \
                info.AddPropertyPODTyped(                                               \
                    #PropName, RTBEngine::Reflection::PropertyType::ComponentRef,        \
                    RTB_MEMBER_OFFSET(PropName), sizeof(void*),                         \
                    RTBEngine::Reflection::PropertyFlags::None, #ComponentType           \
                );                                                                      \
                RTB__BRIDGE_PROP(                                                       \
                    #PropName, RTBEngine::Reflection::PropertyType::ComponentRef,        \
                    RTB_MEMBER_OFFSET(PropName), sizeof(void*),                         \
                    RTBEngine::Reflection::PropertyFlags::None,                         \
                    0.0f, 0.0f, 0, nullptr, #ComponentType                              \
                );
#endif

// Forward declaration visible to all script .cpp files when building GameScripts.dll.
#ifdef GAMESCRIPTS_EXPORTS
extern "C" void RTBScripts_RegisterLocalType(const RTBScriptTypeDesc* desc);
extern "C" void RTBScripts_RegisterLocalProperty(const char* ownerType, const RTBPropertyDesc* desc);
#endif

// Ends property registration - pass ClassName again
// When compiling GameScripts.dll, register into the local POD list (no STL across boundary).
// Otherwise register directly into the engine TypeRegistry.
#ifdef GAMESCRIPTS_EXPORTS
#define RTB_END_REGISTER(ClassName)                                                          \
            RTBScriptTypeDesc _rtb_type_desc{};                                              \
            _rtb_type_desc.typeName = #ClassName;                                            \
            _rtb_type_desc.createComponent = []() -> void* {                                  \
                return static_cast<RTBEngine::ECS::Component*>(new ClassName());              \
            };                                                                                \
            _rtb_type_desc.destroyComponent = [](void* component) {                           \
                delete static_cast<ClassName*>(static_cast<RTBEngine::ECS::Component*>(component)); \
            };                                                                                \
            _rtb_type_desc.instanceSize = sizeof(ClassName);                                \
            RTBScripts_RegisterLocalType(&_rtb_type_desc);                                    \
        }                                                                                     \
    };                                                                                        \
    namespace { static ClassName##_TypeRegistrar _##ClassName##_registrar; }
#else
#define RTB_END_REGISTER(ClassName)                                                                     RTBEngine::Reflection::TypeRegistry::GetInstance().RegisterType(                             #ClassName, ClassName::MutableTypeInfo());                                        }                                                                                   };                                                                                   namespace { static ClassName##_TypeRegistrar _##ClassName##_registrar; }
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
