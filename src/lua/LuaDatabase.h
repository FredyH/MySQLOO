
#ifndef MYSQLOO_LUADATABASE_H
#define MYSQLOO_LUADATABASE_H


#include "../mysql/Database.h"

#include <utility>
#include "LuaObject.h"

class LuaDatabase : public LuaObject {
public:
    static void createMetaTable(ILuaBase *LUA);

    static int create(lua_State *L);

    void think(ILuaBase *lua);

    int m_tableReference = 0;
    std::shared_ptr<Database> m_database;

    static std::shared_ptr<LuaDatabase> create(std::shared_ptr<Database> database) {
        auto instance = std::shared_ptr<LuaDatabase>(new LuaDatabase(std::move(database)));
        LuaObject::luaObjects.insert(instance);
        LuaObject::luaDatabases.insert(instance);
        return instance;
    }
    bool m_dbCallbackRan = false;

    void onDestroyedByLua(ILuaBase *LUA) override;

protected:
    explicit LuaDatabase(std::shared_ptr<Database> database) : LuaObject("Database"),
                                                               m_database(std::move(database)) {

    }
};


#endif //MYSQLOO_LUADATABASE_H
