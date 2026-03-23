#include "ScenePropertySerializer.h"
#include "../ECS/Component.h"
#include "../ECS/GameObject.h"
#include "../Reflection/TypeInfo.h"
#include "../Core/ResourceManager.h"
#include "../Math/Vectors/Vector2.h"
#include "../Math/Vectors/Vector3.h"
#include "../Math/Vectors/Vector4.h"
#include "../Math/Quaternions/Quaternion.h"
#include "../Math/Color.h"
#include "../Animation/Animator.h"
#include "../RTBEngine.h"
#include <sstream>
#include <iomanip>

namespace RTBEngine {
    namespace Scripting {
        namespace ScenePropertySerializer {

            void WriteComponent(std::ofstream& file, const ECS::Component* comp, int indent)
            {
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

                if (std::string(typeName) == "Animator") {
                    const auto* animator = static_cast<const Animation::Animator*>(comp);
                    if (!animator->additionalModels.empty()) {
                        file << ",\n";
                        file << ind << "    additionalModels = {\n";
                        for (const auto& path : animator->additionalModels) {
                            file << ind << "        " << FormatString(NormalizePath(path)) << ",\n";
                        }
                        file << ind << "    }";
                    }
                }

                file << "\n" << ind << "},\n";
            }

            void WriteProperty(std::ofstream& file, const ECS::Component* comp,
                const Reflection::PropertyInfo& prop, int indent)
            {
                std::string ind = Indent(indent);
                void* data = (char*)comp->GetActualObject() + prop.offset;

                file << ind << prop.name << " = ";

                switch (prop.type) {
                case Reflection::PropertyType::Bool:
                    file << FormatBool(*(bool*)data);
                    break;
                case Reflection::PropertyType::Int:
                    file << *(int*)data;
                    break;
                case Reflection::PropertyType::Float:
                    file << std::fixed << std::setprecision(2) << *(float*)data;
                    break;
                case Reflection::PropertyType::Double:
                    file << std::fixed << std::setprecision(2) << *(double*)data;
                    break;
                case Reflection::PropertyType::String:
                    file << FormatString(NormalizePath(*(std::string*)data));
                    break;
                case Reflection::PropertyType::Vector2:
                    file << FormatVector2(*(Math::Vector2*)data);
                    break;
                case Reflection::PropertyType::Vector3:
                    file << FormatVector3(*(Math::Vector3*)data);
                    break;
                case Reflection::PropertyType::Vector4:
                    file << FormatVector4(*(Math::Vector4*)data);
                    break;
                case Reflection::PropertyType::Color: {
                    const Math::Color& c = *(Math::Color*)data;
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(2);
                    oss << "Color(" << c.r << ", " << c.g << ", " << c.b << ", " << c.a << ")";
                    file << oss.str();
                    break;
                }
                case Reflection::PropertyType::Quaternion:
                    file << FormatQuaternion(*(Math::Quaternion*)data);
                    break;
                case Reflection::PropertyType::TextureRef:
                case Reflection::PropertyType::AudioClipRef:
                case Reflection::PropertyType::MeshRef:
                case Reflection::PropertyType::FontRef:
                case Reflection::PropertyType::AssetRef: {
                    void* resourcePtr = *(void**)data;
                    if (!resourcePtr) { file << "nil"; break; }
                    bool isFontRef = (prop.type == Reflection::PropertyType::FontRef);
                    std::string path = GetResourcePath(resourcePtr, isFontRef);
                    file << (path.empty() ? "nil" : FormatString(path));
                    break;
                }
                case Reflection::PropertyType::Enum: {
                    int idx = *(int*)data;
                    if (!prop.enumNames.empty() && idx >= 0 && idx < (int)prop.enumNames.size())
                        file << FormatString(prop.enumNames[idx]);
                    else
                        file << idx;
                    break;
                }
                case Reflection::PropertyType::GameObjectRef: {
                    ECS::GameObject* target = *(ECS::GameObject**)data;
                    file << (target ? FormatString(target->GetUUID()) : "nil");
                    break;
                }
                case Reflection::PropertyType::ComponentRef: {
                    ECS::Component* target = *(ECS::Component**)data;
                    if (target && target->GetOwner()) {
                        std::string ref = target->GetOwner()->GetUUID()
                            + "/" + std::string(target->GetTypeName());
                        file << FormatString(ref);
                    }
                    else {
                        file << "nil";
                    }
                    break;
                }
                default:
                    file << "nil";
                    break;
                }
            }

            std::string FormatVector2(const Math::Vector2& v) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2);
                oss << "Vector2(" << v.x << ", " << v.y << ")";
                return oss.str();
            }

            std::string FormatVector3(const Math::Vector3& v) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2);
                oss << "Vector3(" << v.x << ", " << v.y << ", " << v.z << ")";
                return oss.str();
            }

            std::string FormatVector4(const Math::Vector4& v) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2);
                oss << "Vector4(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
                return oss.str();
            }

            std::string FormatQuaternion(const Math::Quaternion& q) {
                Math::Vector3 eulerRad = q.ToEulerAngles();
                const float rad2deg = 180.0f / 3.14159265f;
                Math::Vector3 eulerDeg(eulerRad.x * rad2deg, eulerRad.y * rad2deg, eulerRad.z * rad2deg);
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2);
                oss << "Quaternion.FromEulerAngles(" << eulerDeg.x << ", " << eulerDeg.y << ", " << eulerDeg.z << ")";
                return oss.str();
            }

            std::string FormatBool(bool b) {
                return b ? "true" : "false";
            }

            std::string FormatString(const std::string& s) {
                return "\"" + s + "\"";
            }

            std::string GetResourcePath(void* resourcePtr, bool silentOnFailure) {
                if (!resourcePtr) return "";
                Core::ResourceManager& rm = Core::ResourceManager::GetInstance();
                std::string path;
                path = rm.GetTexturePath(static_cast<Rendering::Texture*>(resourcePtr));
                if (!path.empty()) return NormalizePath(path);
                path = rm.GetAudioClipPath(static_cast<Audio::AudioClip*>(resourcePtr));
                if (!path.empty()) return NormalizePath(path);
                path = rm.GetMeshPath(static_cast<Rendering::Mesh*>(resourcePtr));
                if (!path.empty()) return NormalizePath(path);
                path = rm.GetFontPath(static_cast<Rendering::Font*>(resourcePtr));
                if (!path.empty()) return NormalizePath(path);
                path = rm.GetCubemapPath(static_cast<Rendering::Cubemap*>(resourcePtr));
                if (!path.empty()) return NormalizePath(path);
                return "";
            }

            std::string NormalizePath(const std::string& path) {
                std::string result = path;
                for (char& c : result) if (c == '\\') c = '/';
                return result;
            }

            std::string Indent(int level) {
                return std::string(level * 4, ' ');
            }

        }
    }
}
