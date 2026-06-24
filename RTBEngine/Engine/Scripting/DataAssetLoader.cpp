#include "DataAssetLoader.h"

#include "DataAssetReflectionUtils.h"
#include "SceneLuaBindings.h"

#include "../Core/Logger.h"
#include "../Core/ResourceManager.h"
#include "../Data/DataAsset.h"
#include "../Data/DataAssetRegistry.h"

#include <lua.hpp>

namespace RTBEngine {
    namespace Scripting {

        std::unique_ptr<Data::DataAsset> DataAssetLoader::Load(const std::string& filePath)
        {
            const std::string resolvedPath =
                Core::ResourceManager::GetInstance().ResolvePathForRead(filePath);

            lua_State* L = luaL_newstate();
            luaL_openlibs(L);
            SceneLuaBindings::SetupLuaBindings(L);

            if (luaL_dofile(L, resolvedPath.c_str()) != LUA_OK) {
                RTB_ERROR("DataAssetLoader: Failed to load '" + filePath + "': "
                    + std::string(lua_tostring(L, -1)));
                lua_close(L);
                return nullptr;
            }

            if (!lua_istable(L, -1)) {
                RTB_ERROR("DataAssetLoader: File does not return a table: " + filePath);
                lua_close(L);
                return nullptr;
            }

            const int rootTableIndex = lua_gettop(L);

            lua_getfield(L, rootTableIndex, "type");
            if (!lua_isstring(L, -1)) {
                RTB_ERROR("DataAssetLoader: Missing 'type' field in: " + filePath);
                lua_close(L);
                return nullptr;
            }

            const std::string typeName = lua_tostring(L, -1);
            lua_pop(L, 1);

            Data::DataAsset* asset = Data::DataAssetRegistry::GetInstance().Create(typeName);
            if (!asset) {
                lua_close(L);
                return nullptr;
            }

            DataAssetReflectionUtils::ApplyLuaTableToDataAsset(L, rootTableIndex, asset);
            asset->SetSourcePath(filePath);

            lua_close(L);
            return std::unique_ptr<Data::DataAsset>(asset);
        }

    }
}
