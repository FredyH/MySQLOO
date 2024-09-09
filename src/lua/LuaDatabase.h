
#ifndef MYSQLOO_LUADATABASE_H
#define MYSQLOO_LUADATABASE_H


#include "../mysql/Database.h"

#include <utility>
#include "LuaObject.h"

class LuaDatabase : public LuaObject {
public:
    static void createMetaTable(ILuaBase *LUA);

    static int create(lua_State *L);

    void think(ILuaBase *LUA);

    int m_tableReference = 0;
    bool m_hasOnDisconnected = false;
    std::shared_ptr<Database> m_database;
    bool m_dbCallbackRan = false;

    void onDestroyedByLua(ILuaBase *LUA) override;

    explicit LuaDatabase(std::shared_ptr<Database> database) : LuaObject("Database"),
                                                               m_database(std::move(database)) {
    }

    static void createWeakTable(ILuaBase *LUA);
    static void runAllThinkHooks(ILuaBase *LUA);
};


#endif //MYSQLOO_LUADATABASE_H
