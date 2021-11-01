#include <algorithm>
#include "LuaObject.h"
#include "LuaDatabase.h"

int LuaObject::TYPE_DATABASE = 0;
int LuaObject::TYPE_QUERY = 0;
int LuaObject::TYPE_TRANSACTION = 0;
int LuaObject::TYPE_PREPARED_QUERY = 0;

std::deque<std::shared_ptr<LuaObject>> LuaObject::luaObjects = {};
std::deque<std::shared_ptr<LuaDatabase>> LuaObject::luaDatabases = {};

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
    LuaObject::luaDatabases.erase(
            std::remove(LuaObject::luaDatabases.begin(), LuaObject::luaDatabases.end(), luaObject),
            LuaObject::luaDatabases.end()
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
    std::deque<std::shared_ptr<LuaDatabase>> databasesCopy = LuaObject::luaDatabases;
    for (auto &database: databasesCopy) {
        database->think(LUA);
    }
    return 0;
}


void LuaObject::addMetaTableFunctions(GarrysMod::Lua::ILuaBase *lua) {
    lua->Push(-1);
    lua->SetField(-2, "__index");

    lua->PushCFunction(luaObjectGc);
    lua->SetField(-2, "__gc");

    lua->PushCFunction(luaObjectToString);
    lua->SetField(-2, "__tostring");
}

LUA_FUNCTION(errorReporter) {
    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->GetField(-1, "debug");
    LUA->GetField(-1, "traceback");

    if (LUA->IsType(-1, GarrysMod::Lua::Type::Function)) {
        LUA->Push(1);
        LUA->Call(1, 1);
    } else {
        LUA->Pop(3); // global, traceback
    }

    return 1;
}

void LuaObject::pcallWithErrorReporter(ILuaBase *LUA, int nargs, int nresults) {
    LUA->PushCFunction(errorReporter);
    int errorHandlerIndex = LUA->Top() - nargs - 2;
    LUA->Insert(-nargs - 2);
    LUA->PCall(nargs, nresults, errorHandlerIndex);
    LUA->Pop(); //Error reporter
}

bool LuaObject::getCallbackReference(ILuaBase *LUA, int functionReference, int tableReference,
                                     const std::string &callbackName, bool allowCallback) {
    //Push function reference
    if (functionReference != 0) {
        LUA->ReferencePush(functionReference);
    } else if (allowCallback && tableReference != 0) {
        LUA->ReferencePush(tableReference);
        LUA->GetField(-1, callbackName.c_str());
        LUA->Remove(-2);
        if (LUA->IsType(-1, GarrysMod::Lua::Type::Function)) {
            return true;
        } else {
            LUA->Pop(1); //The field that is either nil or some other weird thing
            return false;
        }
    } else {
        return false;
    }
    return true;
}
