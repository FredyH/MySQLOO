
#ifndef MYSQLOO_LUAPREPAREDQUERY_H
#define MYSQLOO_LUAPREPAREDQUERY_H


#include "LuaQuery.h"
#include "../mysql/PreparedQuery.h"

class LuaPreparedQuery : public LuaQuery {
public:
    std::shared_ptr<IQueryData> buildQueryData(ILuaBase *LUA, int stackPosition) override;

    static void createMetaTable(ILuaBase *LUA);

    static std::shared_ptr<LuaPreparedQuery> create(const std::shared_ptr<PreparedQuery> &query, int databaseRef) {
        auto instance = std::shared_ptr<LuaPreparedQuery>(new LuaPreparedQuery(query, databaseRef));
        LuaObject::luaObjects.insert(instance);
        return instance;
    }

protected:
    explicit LuaPreparedQuery(const std::shared_ptr<PreparedQuery> &query, int databaseRef) : LuaQuery(
            std::dynamic_pointer_cast<Query>(query), "MySQLOO Prepared Query", databaseRef) {

    }
};


#endif //MYSQLOO_LUAPREPAREDQUERY_H
