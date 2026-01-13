#pragma once
#include <string>
#include <fstream>

namespace RTBEngine {
    namespace ECS {
        class Scene;
        class GameObject;
        class Component;
    }
    namespace Reflection {
        struct PropertyInfo;
    }
    namespace Math {
        class Vector2;
        class Vector3;
        class Vector4;
        class Quaternion;
    }
}

namespace RTBEngine {
    namespace Scripting {

        class SceneSaver {
        public:
            // Main method to save a complete scene to Lua file
            static bool SaveScene(const ECS::Scene* scene, const std::string& filePath);

        private:
            // Write scene header (name, skybox settings)
            static void WriteSceneHeader(std::ofstream& file, const ECS::Scene* scene);

            // Write all GameObjects in the scene
            static void WriteGameObjects(std::ofstream& file, const ECS::Scene* scene);

            // Write a single GameObject (recursive for children)
            static void WriteGameObject(std::ofstream& file, const ECS::GameObject* go, int indent);

            // Write GameObject's Transform data
            static void WriteTransform(std::ofstream& file, const ECS::GameObject* go, int indent);

            // Write all components of a GameObject
            static void WriteComponents(std::ofstream& file, const ECS::GameObject* go, int indent);

            // Write a single component using Reflection system
            static void WriteComponent(std::ofstream& file, const ECS::Component* comp, int indent);

            // Write a single property from a component
            static void WriteProperty(std::ofstream& file, const ECS::Component* comp,
                const Reflection::PropertyInfo& prop, int indent);

            // Helpers to format C++ types to Lua string format
            static std::string FormatVector2(const Math::Vector2& v);
            static std::string FormatVector3(const Math::Vector3& v);
            static std::string FormatVector4(const Math::Vector4& v);
            static std::string FormatQuaternion(const Math::Quaternion& q);
            static std::string FormatBool(bool b);
            static std::string FormatString(const std::string& s);

            // Helper to get file path from resource pointer (for serialization)
            static std::string GetResourcePath(void* resourcePtr);

            // Helper to generate indentation spaces
            static std::string Indent(int level);
        };

    }
}
