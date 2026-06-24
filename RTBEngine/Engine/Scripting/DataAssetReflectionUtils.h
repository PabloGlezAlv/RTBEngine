#pragma once

struct lua_State;

namespace RTBEngine {
    namespace Data {
        class DataAsset;
    }

    namespace Scripting {
        namespace DataAssetReflectionUtils {

            void ApplyLuaTableToDataAsset(lua_State* L, int tableIndex, Data::DataAsset* asset);

        }
    }
}
