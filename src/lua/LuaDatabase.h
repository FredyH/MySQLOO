//
// Created by Fredy on 28/10/2021.
//

#ifndef MYSQLOO_LUADATABASE_H
#define MYSQLOO_LUADATABASE_H


#include "../mysql/Database.h"

#include <utility>
#include "LuaObject.h"

class LuaDatabase : public LuaObject {
public:
    static void createMetaTable(ILuaBase *LUA);

    void think(ILuaBase *lua) override;

    int m_tableReference = 0;
    std::shared_ptr<Database> m_database;
protected:
    explicit LuaDatabase(std::shared_ptr<Database> database) : LuaObject("Database"),
                                                               m_database(std::move(database)) {

    }
};


#endif //MYSQLOO_LUADATABASE_H
