#include "DataAssetSaver.h"

#include "ScenePropertySerializer.h"

#include "../Core/Logger.h"
#include "../Core/ResourceManager.h"
#include "../Data/DataAsset.h"
#include "../Math/Color.h"
#include "../Math/Quaternions/Quaternion.h"
#include "../Reflection/TypeInfo.h"

#include <fstream>

namespace RTBEngine {
    namespace Scripting {

        bool DataAssetSaver::Save(const std::string& filePath, const Data::DataAsset& asset)
        {
            const Reflection::TypeInfo* typeInfo = asset.GetTypeInfo();
            if (!typeInfo) {
                RTB_ERROR("DataAssetSaver: Missing TypeInfo for data asset.");
                return false;
            }

            const std::string resolvedPath =
                Core::ResourceManager::GetInstance().ResolvePathForRead(filePath);

            std::ofstream file(resolvedPath, std::ios::trunc);
            if (!file.is_open()) {
                RTB_ERROR("DataAssetSaver: Failed to open '" + filePath + "' for writing.");
                return false;
            }

            const void* objectBase = asset.GetActualObject();
            const std::string indent = ScenePropertySerializer::Indent(1);

            file << "return {\n";
            file << indent << "type = \"" << asset.GetTypeName() << "\"";

            for (const Reflection::PropertyInfo* prop : typeInfo->GetSerializableProperties()) {
                if (!prop || prop->name == "type") {
                    continue;
                }

                file << ",\n";
                file << indent << prop->name << " = ";

                const void* data = prop->GetData(objectBase);
                switch (prop->type) {
                case Reflection::PropertyType::Bool:
                    file << ScenePropertySerializer::FormatBool(*static_cast<const bool*>(data));
                    break;
                case Reflection::PropertyType::Int:
                    file << *static_cast<const int*>(data);
                    break;
                case Reflection::PropertyType::Float:
                    file << *static_cast<const float*>(data);
                    break;
                case Reflection::PropertyType::Double:
                    file << *static_cast<const double*>(data);
                    break;
                case Reflection::PropertyType::String:
                case Reflection::PropertyType::AssetRef:
                    file << ScenePropertySerializer::FormatString(
                        ScenePropertySerializer::NormalizePath(*static_cast<const std::string*>(data)));
                    break;
                case Reflection::PropertyType::Vector2:
                    file << ScenePropertySerializer::FormatVector2(*static_cast<const Math::Vector2*>(data));
                    break;
                case Reflection::PropertyType::Vector3:
                    file << ScenePropertySerializer::FormatVector3(*static_cast<const Math::Vector3*>(data));
                    break;
                case Reflection::PropertyType::Vector4:
                    file << ScenePropertySerializer::FormatVector4(*static_cast<const Math::Vector4*>(data));
                    break;
                case Reflection::PropertyType::Color: {
                    const Math::Color& color = *static_cast<const Math::Color*>(data);
                    file << "Color(" << color.r << ", " << color.g << ", " << color.b << ", " << color.a << ")";
                    break;
                }
                case Reflection::PropertyType::Quaternion:
                    file << ScenePropertySerializer::FormatQuaternion(*static_cast<const Math::Quaternion*>(data));
                    break;
                case Reflection::PropertyType::Enum: {
                    const int enumIndex = *static_cast<const int*>(data);
                    if (!prop->enumNames.empty() && enumIndex >= 0
                        && enumIndex < static_cast<int>(prop->enumNames.size())) {
                        file << ScenePropertySerializer::FormatString(prop->enumNames[enumIndex]);
                    }
                    else {
                        file << enumIndex;
                    }
                    break;
                }
                default:
                    file << "nil";
                    break;
                }
            }

            file << "\n}\n";
            return true;
        }

    }
}
