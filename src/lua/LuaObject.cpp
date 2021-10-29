#include <algorithm>
#include "LuaObject.h"

std::string LuaObject::toString() {
    std::stringstream ss;
    ss << s_className << " " << this;
    return ss.str();
}

std::shared_ptr<LuaObject> LuaObject::checkMySQLOOType(ILuaBase *lua, int position) {
    int type = lua->GetType(position);
    if (type != LuaObject::TYPE_DATABASE && type != LuaObject::TYPE_PREPARED_QUERY &&
        type != LuaObject::TYPE_QUERY && type != LuaObject::TYPE_TRANSACTION) {
        lua->ThrowError("Provided argument is not a valid MySQLOO object");
    }
    return lua->GetUserType<LuaObject>(position, type)->shared_from_this();
}

LUA_FUNCTION(luaObjectGc) {
    auto luaObject = LuaObject::checkMySQLOOType(LUA);
    LuaObject::luaThinkObjects.push_back(luaObject);
    LuaObject::luaThinkObjects.erase(
            std::remove(LuaObject::luaThinkObjects.begin(), LuaObject::luaThinkObjects.end(), luaObject),
            LuaObject::luaThinkObjects.end()
    );
    LuaObject::luaObjects.erase(
            std::remove(LuaObject::luaObjects.begin(), LuaObject::luaObjects.end(), luaObject),
            LuaObject::luaObjects.end()
    );
    //After this function this object should be deleted.
    //For the Database this might cause the database thread to join
    //TODO: Figure out if this is wise.
    return 0;
}

LUA_FUNCTION(luaObjectToString) {
    auto luaObject = LuaObject::checkMySQLOOType(LUA);
    auto str = luaObject->toString();
    LUA->PushString(str.c_str());
    return 1;
}

LUA_CLASS_FUNCTION(LuaObject, luaObjectThink) {
    std::deque<std::shared_ptr<LuaObject>> thinkObjectsCopy = LuaObject::luaThinkObjects;
    for (auto &query: thinkObjectsCopy) {
        query->think(LUA);
    }
    return 0;
}


void LuaObject::addMetaTableFunctions(GarrysMod::Lua::ILuaBase *lua) {
    lua->CreateTable();
    lua->SetField(-2, "__index");

    lua->PushCFunction(luaObjectGc);
    lua->SetField(-2, "__gc");

    lua->PushCFunction(luaObjectToString);
    lua->SetField(-2, "__tostring");
}