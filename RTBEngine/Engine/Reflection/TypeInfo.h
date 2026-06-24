#pragma once
#include "../RTBEngineAPI.h"
#include "../Scene/Component.h"
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <unordered_map>

namespace RTBEngine {
    namespace ECS {
        class Component;
    }

    namespace Reflection {

        // Supported property types for reflection
        enum class PropertyType {
            Bool,
            Int,
            Float,
            Double,
            String,
            Vector2,
            Vector3,
            Vector4,
            Quaternion,
            Color,
            Enum,
            AssetRef,
            TextureRef,      // Reference to Texture*
            AudioClipRef,    // Reference to AudioClip*
            MeshRef,         // Reference to Mesh*
            FontRef,         // Reference to Font*
            GameObjectRef,   // Reference to GameObject*
            ComponentRef,    // Reference to Component*
            List,            // std::vector<T> / std::vector<T*> — see ListElementType
            Unknown
        };

        // Element type for PropertyType::List (vector-backed serializable lists).
        enum class ListElementType : int {
            None = 0,
            String = 1,
            AssetRef = 2,
            GameObjectRef = 3,
            ComponentRef = 4,
        };

        // Property configuration flags
        enum class PropertyFlags : uint32_t {
            None            = 0,
            Serialize       = 1 << 0,   // 0001 Private var marked with RTB_SERIALIZE
            HideInInspector = 1 << 1,   // 0010 Hide from inspector even if public
            ReadOnly        = 1 << 2,   // 0100 Read-only in inspector
        };

        // Flag operators
        inline PropertyFlags operator|(PropertyFlags a, PropertyFlags b) {
            return static_cast<PropertyFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
        }

        inline PropertyFlags operator&(PropertyFlags a, PropertyFlags b) {
            return static_cast<PropertyFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
        }

        inline bool HasFlag(PropertyFlags flags, PropertyFlags flag) {
            return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
        }

        // Range metadata for numeric sliders
        struct Range {
            float minValue;
            float maxValue;
            Range(float minVal = 0.0f, float maxVal = 1.0f) : minValue(minVal), maxValue(maxVal) {}
        };

        // C4251: STL members in DLL-exported types are safe here — reflection types
        // are used only by internal engine code and not copied across DLL boundaries.
        #pragma warning(push)
        #pragma warning(disable: 4251)

        // Info about a single property
        struct RTB_API PropertyInfo {
            std::string name;
            std::string displayName;
            PropertyType type;
            size_t offset;
            size_t size;
            PropertyFlags flags;

            std::optional<Range> range;
            std::optional<std::string> tooltip;
            std::optional<std::string> category;
            std::vector<std::string> enumNames;
            std::string assetType;
            std::string componentTypeName;  // For ComponentRef / ComponentRef lists
            ListElementType listElementType = ListElementType::None;

            PropertyInfo()
                : type(PropertyType::Unknown)
                , offset(0)
                , size(0)
                , flags(PropertyFlags::None)
            {}

            PropertyInfo(const std::string& name, PropertyType type, size_t offset, size_t size, PropertyFlags flags = PropertyFlags::None)
                : name(name)
                , displayName(name)
                , type(type)
                , offset(offset)
                , size(size)
                , flags(flags)
            {}

            bool IsSerializable() const { return HasFlag(flags, PropertyFlags::Serialize) || !HasFlag(flags, PropertyFlags::HideInInspector); }
            bool IsVisibleInInspector() const { return !HasFlag(flags, PropertyFlags::HideInInspector); }
            bool IsReadOnly() const { return HasFlag(flags, PropertyFlags::ReadOnly); }

            std::string GetInspectorLabel() const;

            void* GetMutableData(void* objectBase) const {
                return objectBase ? static_cast<void*>(static_cast<char*>(objectBase) + offset) : nullptr;
            }

            const void* GetData(const void* objectBase) const {
                return objectBase ? static_cast<const void*>(static_cast<const char*>(objectBase) + offset) : nullptr;
            }

            void* GetMutableData(ECS::Component* component) const;
            const void* GetData(const ECS::Component* component) const;
        };

        // Uses a real temporary instance instead of a fake probe address because
        // MSVC may dereference vbtable data when applying a member pointer on types
        // with multiple inheritance / virtual bases (for example UI event handlers).
        template<typename OwnerClass, typename MemberClass, typename MemberType>
        inline size_t GetMemberOffset(MemberType MemberClass::* member) {
            static_assert(std::is_default_constructible_v<OwnerClass>,
                "Reflected component types must be default constructible.");

            alignas(OwnerClass) unsigned char storage[sizeof(OwnerClass)];
            auto* ownerPtr = new (storage) OwnerClass();
            auto* memberPtr = &(ownerPtr->*member);
            const auto offset = reinterpret_cast<const char*>(memberPtr) - reinterpret_cast<const char*>(ownerPtr);
            ownerPtr->~OwnerClass();
            return static_cast<size_t>(offset);
        }

        class RTB_API TypeInfo {
        public:
            // Raw function pointer — POD, no destructor, safe to copy across /MT module boundaries.
            using FactoryFunc  = ECS::Component*(*)(void* context);
            // Raw function pointer — POD, no destructor, safe to copy across /MT module boundaries.
            using DestroyFunc  = void(*)(ECS::Component*, void* context);

            TypeInfo() = default;
            TypeInfo(const char* typeName, FactoryFunc factory = nullptr);

            const std::string& GetTypeName() const { return typeName; }
            const std::vector<PropertyInfo>& GetProperties() const { return properties; }
            const PropertyInfo* GetProperty(const std::string& name) const;
            std::vector<const PropertyInfo*> GetInspectorProperties() const;
            std::vector<const PropertyInfo*> GetSerializableProperties() const;
            void AddProperty(const PropertyInfo& prop);
            void AddPropertyPOD(const char* name, PropertyType type, size_t offset, size_t size, PropertyFlags flags);
            void AddPropertyPODRange(const char* name, PropertyType type, size_t offset, size_t size, PropertyFlags flags, float rangeMin, float rangeMax);
            void AddPropertyPODEnum(const char* name, size_t offset, size_t size, PropertyFlags flags, const char* const* enumNames, int enumCount);
            void AddPropertyPODTyped(const char* name, PropertyType type, size_t offset, size_t size, PropertyFlags flags, const char* extraTypeName);
            void AddPropertyList(const char* name, ListElementType elementType, size_t offset, size_t size,
                PropertyFlags flags, const char* componentTypeName = nullptr);
            bool HasProperties() const { return !properties.empty(); }
            size_t GetPropertyCount() const { return properties.size(); }

            void SetIsDataAsset(bool value) { isDataAsset = value; }
            bool IsDataAsset() const { return isDataAsset; }

            ECS::Component* Create() const { return factoryFn ? factoryFn(factoryCtx) : nullptr; }
            void SetFactory(FactoryFunc fn, void* ctx) { factoryFn = fn; factoryCtx = ctx; }

            // Destroys a component using the heap that allocated it.
            // Script components are created with new inside GameScripts.dll (/MT heap).
            // Calling delete from a different module crashes; the destroyer runs in the
            // module that allocated, using a raw fn ptr + opaque context (both POD).
            void Destroy(ECS::Component* c) const { if (destroyFn) destroyFn(c, destroyCtx); else delete c; }
            void SetDestroyer(DestroyFunc fn, void* ctx) { destroyFn = fn; destroyCtx = ctx; }

        private:
            std::string typeName;
            std::vector<PropertyInfo> properties;
            FactoryFunc factoryFn  = nullptr;
            void*       factoryCtx = nullptr;
            DestroyFunc destroyFn  = nullptr;
            void*       destroyCtx = nullptr;
            bool        isDataAsset = false;
        };

        // Global registry for all reflected types
        class RTB_API TypeRegistry {
        public:
            static TypeRegistry& GetInstance();

            void RegisterType(const std::string& typeName, const TypeInfo& info);
            void UnregisterType(const std::string& typeName);
            const TypeInfo* GetTypeInfo(const std::string& typeName) const;
            const TypeInfo* GetTypeInfo(const char* typeName) const;
            bool HasType(const std::string& typeName) const;
            std::vector<std::string> GetRegisteredTypes() const;
            void ForEachType(void(*callback)(const char* typeName, const TypeInfo* info, void* userData), void* userData) const;
            ECS::Component* CreateComponent(const std::string& typeName) const;

        private:
            TypeRegistry() = default;
            TypeRegistry(const TypeRegistry&) = delete;
            TypeRegistry& operator=(const TypeRegistry&) = delete;

            std::unordered_map<std::string, TypeInfo> types;
        };
        #pragma warning(pop)

    }
}
