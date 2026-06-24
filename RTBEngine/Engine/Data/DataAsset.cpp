#include "DataAsset.h"

#include "../Reflection/TypeInfo.h"

namespace RTBEngine {
    namespace Data {

        const Reflection::TypeInfo* DataAsset::GetTypeInfo() const
        {
            const char* typeName = GetTypeName();
            if (!typeName || typeName[0] == '\0') {
                return nullptr;
            }

            return Reflection::TypeRegistry::GetInstance().GetTypeInfo(typeName);
        }

    }
}
