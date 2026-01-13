#include "SceneSaver.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/Component.h"
#include "../ECS/Transform.h"
#include "../Math/Vectors/Vector2.h"
#include "../Math/Vectors/Vector3.h"
#include "../Math/Vectors/Vector4.h"
#include "../Math/Quaternions/Quaternion.h"
#include "../Reflection/TypeInfo.h"
#include "../Core/ResourceManager.h"
#include "../RTBEngine.h"
#include <sstream>
#include <iomanip>

namespace RTBEngine {
    namespace Scripting {

        bool SceneSaver::SaveScene(const ECS::Scene* scene, const std::string& filePath) {
            if (!scene) {
                RTB_ERROR("SceneSaver: Cannot save null scene");
                return false;
            }

            std::ofstream file(filePath);
            if (!file.is_open()) {
                RTB_ERROR("SceneSaver: Failed to open file for writing: " + filePath);
                return false;
            }

            try {
                file << "function CreateScene()\n";
                file << "    return {\n";

                WriteSceneHeader(file, scene);
                WriteGameObjects(file, scene);

                file << "    }\n";
                file << "end\n";

                file.close();
                RTB_INFO("SceneSaver: Successfully saved scene to: " + filePath);
                return true;

            }
            catch (const std::exception& e) {
                RTB_ERROR("SceneSaver: Exception during save: " + std::string(e.what()));
                return false;
            }
        }

        void SceneSaver::WriteSceneHeader(std::ofstream& file, const ECS::Scene* scene) {
            file << "        name = \"" << scene->GetName() << "\",\n";
            file << "        skyboxEnabled = " << FormatBool(scene->IsSkyboxEnabled()) << ",\n";
        }

        void SceneSaver::WriteGameObjects(std::ofstream& file, const ECS::Scene* scene) {
            const auto& gameObjects = scene->GetGameObjects();

            file << "        gameObjects = {\n";

            for (const auto& go : gameObjects) {
                if (go->GetParent() == nullptr) {
                    WriteGameObject(file, go.get(), 3);
                }
            }

            file << "        }\n";
        }

        void SceneSaver::WriteGameObject(std::ofstream& file, const ECS::GameObject* go, int indent) {
            std::string ind = Indent(indent);

            file << ind << "{\n";
            file << ind << "    name = \"" << go->GetName() << "\",\n";

            WriteTransform(file, go, indent + 1);
            WriteComponents(file, go, indent + 1);

            const auto& children = go->GetChildren();
            if (!children.empty()) {
                file << ind << "    children = {\n";
                for (const auto* child : children) {
                    WriteGameObject(file, child, indent + 2);
                }
                file << ind << "    }\n";
            }

            file << ind << "},\n";
        }

        void SceneSaver::WriteTransform(std::ofstream& file, const ECS::GameObject* go, int indent) {
            std::string ind = Indent(indent);
            const auto& transform = go->GetTransform();

            Math::Vector3 pos = transform.GetPosition();
            Math::Vector3 rot = transform.GetRotation().ToEulerAngles();
            Math::Vector3 scale = transform.GetScale();

            if (pos.x != 0.0f || pos.y != 0.0f || pos.z != 0.0f) {
                file << ind << "position = " << FormatVector3(pos) << ",\n";
            }

            if (rot.x != 0.0f || rot.y != 0.0f || rot.z != 0.0f) {
                file << ind << "rotation = Quaternion.FromEulerAngles" << FormatVector3(rot) << ",\n";
            }

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) {
                file << ind << "scale = " << FormatVector3(scale) << ",\n";
            }
        }

        void SceneSaver::WriteComponents(std::ofstream& file, const ECS::GameObject* go, int indent) {
            std::string ind = Indent(indent);
            const auto& components = go->GetComponents();

            if (components.empty()) {
                return;
            }

            file << ind << "components = {\n";

            for (const auto& comp : components) {
                WriteComponent(file, comp.get(), indent + 1);
            }

            file << ind << "}\n";
        }

        void SceneSaver::WriteComponent(std::ofstream& file, const ECS::Component* comp, int indent) {
            std::string ind = Indent(indent);
            const char* typeName = comp->GetTypeName();
            const Reflection::TypeInfo* typeInfo = comp->GetTypeInfo();

            file << ind << "{\n";
            file << ind << "    type = \"" << typeName << "\"";

            if (typeInfo) {
                auto properties = typeInfo->GetSerializableProperties();
                for (const auto* prop : properties) {
                    file << ",\n";
                    WriteProperty(file, comp, *prop, indent + 1);
                }
            }

            file << "\n" << ind << "},\n";
        }

        void SceneSaver::WriteProperty(std::ofstream& file, const ECS::Component* comp,
            const Reflection::PropertyInfo& prop, int indent) {
            std::string ind = Indent(indent);
            void* data = (char*)comp + prop.offset;

            file << ind << prop.name << " = ";

            switch (prop.type) {
            case Reflection::PropertyType::Bool:
                file << FormatBool(*(bool*)data);
                break;

            case Reflection::PropertyType::Int:
                file << *(int*)data;
                break;

            case Reflection::PropertyType::Float: {
                float val = *(float*)data;
                file << std::fixed << std::setprecision(2) << val;
                break;
            }

            case Reflection::PropertyType::Double: {
                double val = *(double*)data;
                file << std::fixed << std::setprecision(2) << val;
                break;
            }

            case Reflection::PropertyType::String:
                file << FormatString(*(std::string*)data);
                break;

            case Reflection::PropertyType::Vector2:
                file << FormatVector2(*(Math::Vector2*)data);
                break;

            case Reflection::PropertyType::Vector3:
                file << FormatVector3(*(Math::Vector3*)data);
                break;

            case Reflection::PropertyType::Vector4:
            case Reflection::PropertyType::Color:
                file << FormatVector4(*(Math::Vector4*)data);
                break;

            case Reflection::PropertyType::Quaternion:
                file << FormatQuaternion(*(Math::Quaternion*)data);
                break;

            case Reflection::PropertyType::TextureRef:
            case Reflection::PropertyType::AudioClipRef:
            case Reflection::PropertyType::MeshRef:
            case Reflection::PropertyType::FontRef:
            case Reflection::PropertyType::AssetRef: {
                void* resourcePtr = *(void**)data;
                std::string path = GetResourcePath(resourcePtr);
                file << FormatString(path);
                break;
            }

            case Reflection::PropertyType::Enum:
                file << *(int*)data;
                break;

            default:
                file << "nil";
                break;
            }
        }

        std::string SceneSaver::FormatVector2(const Math::Vector2& v) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2);
            oss << "Vector2(" << v.x << ", " << v.y << ")";
            return oss.str();
        }

        std::string SceneSaver::FormatVector3(const Math::Vector3& v) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2);
            oss << "Vector3(" << v.x << ", " << v.y << ", " << v.z << ")";
            return oss.str();
        }

        std::string SceneSaver::FormatVector4(const Math::Vector4& v) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2);
            oss << "Vector4(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
            return oss.str();
        }

        std::string SceneSaver::FormatQuaternion(const Math::Quaternion& q) {
            Math::Vector3 euler = q.ToEulerAngles();
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2);
            oss << "Quaternion.FromEulerAngles(" << euler.x << ", " << euler.y << ", " << euler.z << ")";
            return oss.str();
        }

        std::string SceneSaver::FormatBool(bool b) {
            return b ? "true" : "false";
        }

        std::string SceneSaver::FormatString(const std::string& s) {
            return "\"" + s + "\"";
        }

        std::string SceneSaver::GetResourcePath(void* resourcePtr) {
            if (!resourcePtr) {
                return "";
            }

            Core::ResourceManager& rm = Core::ResourceManager::GetInstance();
            std::string path;

            path = rm.GetTexturePath(static_cast<Rendering::Texture*>(resourcePtr));
            if (!path.empty()) return path;

            path = rm.GetAudioClipPath(static_cast<Audio::AudioClip*>(resourcePtr));
            if (!path.empty()) return path;

            path = rm.GetMeshPath(static_cast<Rendering::Mesh*>(resourcePtr));
            if (!path.empty()) return path;

            path = rm.GetFontPath(static_cast<Rendering::Font*>(resourcePtr));
            if (!path.empty()) return path;

            path = rm.GetCubemapPath(static_cast<Rendering::Cubemap*>(resourcePtr));
            if (!path.empty()) return path;

            RTB_WARN("SceneSaver: Could not resolve resource path for pointer");
            return "";
        }


        std::string SceneSaver::Indent(int level) {
            return std::string(level * 4, ' ');
        }

    }
}
