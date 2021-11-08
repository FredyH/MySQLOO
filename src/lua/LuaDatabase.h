
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
    std::shared_ptr<Database> m_database;
    bool m_dbCallbackRan = false;

    void onDestroyedByLua(ILuaBase *LUA) override;

    ~LuaDatabase() override {
        luaDatabases->erase(this);
    }

    explicit LuaDatabase(std::shared_ptr<Database> database) : LuaObject("Database"),
                                                               m_database(std::move(database)) {
        luaDatabases->insert(this);
    }

    static std::unordered_set<LuaDatabase*>* luaDatabases;
};


#endif //MYSQLOO_LUADATABASE_H
