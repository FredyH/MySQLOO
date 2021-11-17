
#ifndef MYSQLOO_LUATRANSACTION_H
#define MYSQLOO_LUATRANSACTION_H

#include "LuaIQuery.h"
#include "../mysql/Transaction.h"

class LuaTransaction : public LuaIQuery {
public:
    std::deque<std::shared_ptr<QueryData>> m_addedQueryData = {};

    std::shared_ptr<IQueryData> buildQueryData(ILuaBase *LUA, int stackPosition, bool shouldRef) override;

    static void createMetaTable(ILuaBase *LUA);

    static void runSuccessCallback(ILuaBase *LUA, const std::shared_ptr<Transaction> &transaction,
                                   const std::shared_ptr<TransactionData> &data);

    explicit LuaTransaction(const std::shared_ptr<Transaction> &transaction, int databaseRef) : LuaIQuery(
            std::static_pointer_cast<IQuery>(transaction), "MySQLOO Transaction", databaseRef
    ) {

    }
};


#endif //MYSQLOO_LUATRANSACTION_H
