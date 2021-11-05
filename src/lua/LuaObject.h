

#ifndef MYSQLOO_LUAOBJECT_H
#define MYSQLOO_LUAOBJECT_H

#include <memory>
#include <deque>
#include <utility>
#include <sstream>
#include "GarrysMod/Lua/Interface.h"
#include "../mysql/MySQLOOException.h"

#include <iostream>

class LuaDatabase;

using namespace GarrysMod::Lua;

class LuaObject : public std::enable_shared_from_this<LuaObject> {
public:
    virtual ~LuaObject() = default;

    std::string toString();

    static std::deque<std::shared_ptr<LuaObject>> luaObjects;
    static std::deque<std::shared_ptr<LuaDatabase>> luaDatabases;

    static int TYPE_USERDATA;
    static int TYPE_DATABASE;
    static int TYPE_QUERY;
    static int TYPE_PREPARED_QUERY;
    static int TYPE_TRANSACTION;

    static void addMetaTableFunctions(ILuaBase *LUA);

    static void createUserDataMetaTable(ILuaBase *lua);

    //Lua functions
    static int luaObjectThink(lua_State *L);

    static void pcallWithErrorReporter(ILuaBase *LUA, int nargs);

    static bool pushCallbackReference(ILuaBase *LUA, int functionReference, int tableReference,
                                      const std::string &callbackName, bool allowCallback);

    template<class T>
    static T *getLuaObject(ILuaBase *LUA, int stackPos = 1) {
        LUA->CheckType(stackPos, GarrysMod::Lua::Type::Table);
        LUA->GetField(stackPos, "__CppObject");

        auto *luaObject = LUA->GetUserType<LuaObject>(-1, TYPE_USERDATA);
        if (luaObject == nullptr) {
            LUA->ThrowError("[MySQLOO] Expected MySQLOO table");
        }
        T *returnValue = dynamic_cast<T*>(luaObject);
        if (returnValue == nullptr) {
            LUA->ThrowError("[MySQLOO] Invalid CPP Object");
        }
        LUA->Pop();
        return returnValue;
    }

    static int getFunctionReference(ILuaBase *LUA, int stackPosition, const char *fieldName);

protected:
    explicit LuaObject(std::string className) : m_className(std::move(className)) {

    }

    std::string m_className;
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
