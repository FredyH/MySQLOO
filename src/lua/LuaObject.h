

#ifndef MYSQLOO_LUAOBJECT_H
#define MYSQLOO_LUAOBJECT_H

#include <memory>
#include <deque>
#include <utility>
#include <sstream>
#include "GarrysMod/Lua/Interface.h"
#include "../mysql/MySQLOOException.h"

class LuaDatabase;

using namespace GarrysMod::Lua;

class LuaObject : public std::enable_shared_from_this<LuaObject> {
public:
    std::string toString();

    static std::deque<std::shared_ptr<LuaObject>> luaObjects;
    static std::deque<std::shared_ptr<LuaDatabase>> luaDatabases;

    static std::shared_ptr<LuaObject> checkMySQLOOType(ILuaBase *lua, int position = 1);

    static int TYPE_DATABASE;
    static int TYPE_QUERY;
    static int TYPE_PREPARED_QUERY;
    static int TYPE_TRANSACTION;

    static void addMetaTableFunctions(ILuaBase *lua);

    //Lua functions
    static int luaObjectThink(lua_State *L);

    static void pcallWithErrorReporter(ILuaBase *LUA, int nargs, int nresults);

    static bool getCallbackReference(ILuaBase *LUA, int functionReference, int tableReference,
                                     const std::string &callbackName, bool allowCallback);

    static int getLuaObjectType(ILuaBase *LUA, int stackPos = 1) {
        LUA->CheckType(stackPos, GarrysMod::Lua::Type::Table);
        LUA->GetField(stackPos, "__CppObject");
        int type = LUA->GetType(-1);
        LUA->Pop();
        return type;
    }

    template<typename T>
    static T *getLuaObject(ILuaBase *LUA, int type, int stackPos = 1) {
        LUA->CheckType(stackPos, GarrysMod::Lua::Type::Table);
        LUA->GetField(stackPos, "__CppObject");

        T *returnValue = LUA->GetUserType<T>(-1, type);
        if (returnValue == nullptr) {
            LUA->ThrowError("[MySQLOO] Expected MySQLOO table");
        }
        LUA->Pop();
        return returnValue;
    }
protected:
    explicit LuaObject(std::string className) : s_className(std::move(className)) {

    }

    std::string s_className;
};




#define MYSQLOO_LUA_FUNCTION(FUNC)                          \
            static int FUNC##__Imp( GarrysMod::Lua::ILuaBase* LUA ); \
            static int FUNC( lua_State* L )                          \
            {                                                 \
                GarrysMod::Lua::ILuaBase* LUA = L->luabase;   \
                LUA->SetState(L);                             \
                try {                                         \
                    return FUNC##__Imp( LUA );                \
                } catch (const MySQLOOException& error) {     \
                    LUA->ThrowError(error.message.c_str()); \
                    return 0;                                 \
                }                                             \
            }                                                 \
            static int FUNC##__Imp( GarrysMod::Lua::ILuaBase* LUA )

#define LUA_CLASS_FUNCTION(CLASS, FUNC)                              \
            static int FUNC##__Imp( GarrysMod::Lua::ILuaBase* LUA ); \
            int CLASS::FUNC( lua_State* L )                          \
            {                                                 \
                GarrysMod::Lua::ILuaBase* LUA = L->luabase;   \
                LUA->SetState(L);                             \
                try {                                         \
                    return FUNC##__Imp( LUA );                     \
                } catch (const MySQLOOException& error) {            \
                    LUA->ThrowError(error.message.c_str());            \
                    return 0;                                     \
                }                                                     \
            }                                                 \
            static int FUNC##__Imp( GarrysMod::Lua::ILuaBase* LUA )


#endif //MYSQLOO_LUAOBJECT_H
