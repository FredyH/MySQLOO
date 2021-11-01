
#ifndef MYSQLOO_LUAIQUERY_H
#define MYSQLOO_LUAIQUERY_H

#include "LuaObject.h"
#include "../mysql/IQuery.h"
#include "../mysql/Query.h"

#include <utility>

class LuaIQuery : public LuaObject {
public:
    static void addMetaTableFunctions(ILuaBase *lua);

    std::shared_ptr<IQuery> m_query;

    virtual std::shared_ptr<IQueryData> buildQueryData() = 0;

    static void runAbortedCallback(ILuaBase *LUA, const std::shared_ptr<IQueryData> &data);

    static void runErrorCallback(ILuaBase *LUA, const std::shared_ptr<IQueryData> &data);

    static void runOnDataCallbacks(ILuaBase *LUA, const std::shared_ptr<IQueryData> &data);

    static LuaIQuery *getLuaIQuery(ILuaBase *LUA, int stackPos = 1) {
        int type = LuaObject::getLuaObjectType(LUA, stackPos);
        if (type != LuaObject::TYPE_QUERY && type != LuaObject::TYPE_PREPARED_QUERY &&
            type != LuaObject::TYPE_TRANSACTION) {
            LUA->ThrowError("[MySQLOO] Expected MySQLOO table");
        }
        auto *returnValue = LuaObject::getLuaObject<LuaIQuery>(LUA, type, stackPos);
        if (returnValue == nullptr) {
            LUA->ThrowError("[MySQLOO] Expected MySQLOO table");
        }
        return returnValue;
    }

protected:
    explicit LuaIQuery(std::shared_ptr<IQuery> query, std::string className) : LuaObject(std::move(className)),
                                                                               m_query(std::move(query)) {}
};

#endif //MYSQLOO_LUAIQUERY_H
