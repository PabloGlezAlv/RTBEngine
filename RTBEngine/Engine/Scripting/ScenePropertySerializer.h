#pragma once
#include "../RTBEngineAPI.h"
#include <string>
#include <fstream>

namespace RTBEngine {
    namespace Scene { class Component; }
    namespace Reflection { struct PropertyInfo; }
    namespace Math {
        class Vector2;
        class Vector3;
        class Vector4;
        class Quaternion;
    }
}

namespace RTBEngine {
    namespace Scripting {

        namespace ScenePropertySerializer {

            RTB_API void WriteComponent(std::ofstream& file, const Scene::Component* comp, int indent);
            RTB_API void WriteProperty(std::ofstream& file, const Scene::Component* comp,
                const Reflection::PropertyInfo& prop, int indent);

            RTB_API std::string FormatVector2(const Math::Vector2& v);
            RTB_API std::string FormatVector3(const Math::Vector3& v);
            RTB_API std::string FormatVector4(const Math::Vector4& v);
            RTB_API std::string FormatQuaternion(const Math::Quaternion& q);
            RTB_API std::string FormatBool(bool b);
            RTB_API std::string FormatString(const std::string& s);
            RTB_API std::string GetResourcePath(void* resourcePtr, bool silentOnFailure = false);
            RTB_API std::string NormalizePath(const std::string& path);
            RTB_API std::string Indent(int level);

        }

    }
}
