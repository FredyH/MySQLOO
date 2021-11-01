
#ifndef MYSQLOO_LUAQUERY_H
#define MYSQLOO_LUAQUERY_H


#include "LuaIQuery.h"

class LuaQuery : public LuaIQuery {
public:
    int m_dataReference = 0;

    int createDataReference(ILuaBase *LUA, QueryData &data);

    void runSuccessCallback(ILuaBase *LUA, QueryData &data);

    static void addMetaTableFunctions(ILuaBase *LUA);
    static void createMetaTable(ILuaBase *LUA);

    static LuaQuery *getLuaQuery(ILuaBase *LUA, int stackPos = 1) {
        int type = LuaObject::getLuaObjectType(LUA, stackPos);
        if (type != LuaObject::TYPE_QUERY && type != LuaObject::TYPE_PREPARED_QUERY) {
            LUA->ThrowError("[MySQLOO] Expected MySQLOO query");
        }
        auto *returnValue = LuaObject::getLuaObject<LuaQuery>(LUA, type, stackPos);
        if (returnValue == nullptr) {
            LUA->ThrowError("[MySQLOO] Expected MySQLOO table");
        }
        return returnValue;
    }

};

#endif //MYSQLOO_LUAQUERY_H
