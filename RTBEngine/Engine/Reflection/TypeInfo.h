#pragma once
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <cstddef>
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
            Unknown
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
            float min;
            float max;
            Range(float min = 0.0f, float max = 1.0f) : min(min), max(max) {}
        };

        // Info about a single property
        struct PropertyInfo {
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
            std::string componentTypeName;  // For ComponentRef: target component type name

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
        };

        class TypeInfo {
        public:
            using FactoryFunc = std::function<ECS::Component*()>;

            TypeInfo() = default;
            TypeInfo(const std::string& typeName, FactoryFunc factory = nullptr);

            const std::string& GetTypeName() const { return typeName; }
            const std::vector<PropertyInfo>& GetProperties() const { return properties; }
            const PropertyInfo* GetProperty(const std::string& name) const;
            std::vector<const PropertyInfo*> GetInspectorProperties() const;
            std::vector<const PropertyInfo*> GetSerializableProperties() const;
            void AddProperty(const PropertyInfo& prop);
            bool HasProperties() const { return !properties.empty(); }
            size_t GetPropertyCount() const { return properties.size(); }

            ECS::Component* Create() const { return factory ? factory() : nullptr; }

        private:
            std::string typeName;
            std::vector<PropertyInfo> properties;
            FactoryFunc factory;
        };

        // Global registry for all reflected types
        class TypeRegistry {
        public:
            static TypeRegistry& GetInstance();

            void RegisterType(const std::string& typeName, const TypeInfo& info);
            const TypeInfo* GetTypeInfo(const std::string& typeName) const;
            bool HasType(const std::string& typeName) const;
            std::vector<std::string> GetRegisteredTypes() const;
            ECS::Component* CreateComponent(const std::string& typeName) const;

        private:
            TypeRegistry() = default;
            TypeRegistry(const TypeRegistry&) = delete;
            TypeRegistry& operator=(const TypeRegistry&) = delete;

            std::unordered_map<std::string, TypeInfo> types;
        };

    } 
}
