
#ifndef MYSQLOO_LUATRANSACTION_H
#define MYSQLOO_LUATRANSACTION_H

#include "LuaIQuery.h"

class LuaTransaction : public LuaIQuery {
public:
    std::deque<std::shared_ptr<QueryData>> m_addedQueryData = {};

    std::shared_ptr<IQueryData> buildQueryData(ILuaBase *LUA, int stackPosition) override;

    static void createMetaTable(ILuaBase *LUA);

    static std::shared_ptr<LuaTransaction> create(const std::shared_ptr<Transaction> &transaction, int databaseRef) {
        auto instance = std::shared_ptr<LuaTransaction>(new LuaTransaction(transaction, databaseRef));
        LuaObject::luaObjects.insert(instance);
        return instance;
    }

    void runSuccessCallback(ILuaBase *LUA, const std::shared_ptr<IQueryData> &data) override;

protected:
    explicit LuaTransaction(const std::shared_ptr<Transaction> &transaction, int databaseRef) : LuaIQuery(
            std::static_pointer_cast<IQuery>(transaction), "MySQLOO Transaction", databaseRef
    ) {

    }
};


#endif //MYSQLOO_LUATRANSACTION_H
