#include "LuaTransaction.h"
#include "LuaQuery.h"

MYSQLOO_LUA_FUNCTION(addQuery) {
    auto luaTransaction = LuaObject::getLuaObject<LuaTransaction>(LUA);

    auto addedLuaQuery = LuaQuery::getLuaObject<LuaQuery>(LUA, 2);
    LUA->Push(1);
    LUA->GetField(-1, "__queries");
    if (LUA->IsType(-1, GarrysMod::Lua::Type::Nil)) {
        LUA->Pop();
        LUA->CreateTable();
        LUA->SetField(-2, "__queries");
        LUA->GetField(-1, "__queries");
    }
    int tblIndex = LUA->Top();
    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->GetField(-1, "table");
    LUA->GetField(-1, "insert");
    LUA->Push(tblIndex);
    LUA->Push(2);
    LUA->Call(2, 0);
    LUA->Pop(4);

    auto queryData = std::dynamic_pointer_cast<QueryData>(addedLuaQuery->buildQueryData(LUA, 2, false));

    luaTransaction->m_addedQueryData.push_back(queryData);
    return 0;
}

MYSQLOO_LUA_FUNCTION(getQueries) {
    LUA->GetField(1, "__queries");
    if (LUA->IsType(-1, GarrysMod::Lua::Type::Nil)) {
        LUA->Pop();
        LUA->CreateTable();
    }
    return 1;
}

MYSQLOO_LUA_FUNCTION(clearQueries) {
    auto luaTransaction = LuaObject::getLuaObject<LuaTransaction>(LUA);

    LUA->Push(1);
    LUA->CreateTable();
    LUA->SetField(-2, "__queries");
    LUA->Pop();

    luaTransaction->m_addedQueryData.clear();

    return 0;
}

void LuaTransaction::createMetaTable(ILuaBase *LUA) {
    LuaObject::TYPE_TRANSACTION = LUA->CreateMetaTable("MySQLOO Transaction");

    LuaIQuery::addMetaTableFunctions(LUA);
    LUA->PushCFunction(addQuery);
    LUA->SetField(-2, "addQuery");
    LUA->PushCFunction(getQueries);
    LUA->SetField(-2, "getQueries");
    LUA->PushCFunction(clearQueries);
    LUA->SetField(-2, "clearQueries");
    LUA->Pop();
}

std::shared_ptr<IQueryData> LuaTransaction::buildQueryData(ILuaBase *LUA, int stackPosition, bool shouldRef) {
    LUA->GetField(stackPosition, "__queries");
    std::deque<std::pair<std::shared_ptr<Query>, std::shared_ptr<IQueryData>>> queries;
    if (LUA->GetType(-1) != GarrysMod::Lua::Type::Nil) {
        for (size_t i = 0; i < this->m_addedQueryData.size(); i++) {
            auto &queryData = this->m_addedQueryData[i];
            LUA->PushNumber((double) (i + 1));
            LUA->RawGet(-2);
            if (!LUA->IsType(-1, GarrysMod::Lua::Type::Table)) {
                LUA->Pop(); //Nil or whatever else is on the stack
                break;
            }
            auto luaQuery = LuaQuery::getLuaObject<LuaQuery>(LUA, -1);
            auto query = std::dynamic_pointer_cast<Query>(luaQuery->m_query);
            query->addQueryData(queryData);
            queries.emplace_back(query, queryData);
            LUA->Pop(); //Query
        }
    }
    LUA->Pop(); //Queries table

    auto data = Transaction::buildQueryData(queries);
    if (shouldRef) {
        LuaIQuery::referenceCallbacks(LUA, stackPosition, *data);
    }
    return data;
}

void LuaTransaction::runAbortedCallback(GarrysMod::Lua::ILuaBase *LUA, const std::shared_ptr<Transaction> &transaction,
                                      const std::shared_ptr<TransactionData> &data) {
    auto transactionData = std::dynamic_pointer_cast<TransactionData>(data);
    if (data->m_tableReference == 0) return;
    // Set the correct callback data for the queries of the transaction
    for (auto &pair: transactionData->m_queries) {
        auto query = pair.first;
        auto queryData = std::dynamic_pointer_cast<QueryData>(pair.second);
        query->setCallbackData(pair.second);
    }
    LuaIQuery::runAbortedCallback(LUA, data);
}

void LuaTransaction::runErrorCallback(GarrysMod::Lua::ILuaBase *LUA, const std::shared_ptr<Transaction> &transaction,
                                      const std::shared_ptr<TransactionData> &data) {
    auto transactionData = std::dynamic_pointer_cast<TransactionData>(data);
    if (data->m_tableReference == 0) return;
    // Set the correct callback data for the queries of the transaction
    for (auto &pair: transactionData->m_queries) {
        auto query = pair.first;
        auto queryData = std::dynamic_pointer_cast<QueryData>(pair.second);
        query->setCallbackData(pair.second);
    }
    LuaIQuery::runErrorCallback(LUA, transaction, data);
}

void LuaTransaction::runSuccessCallback(ILuaBase *LUA, const std::shared_ptr<Transaction> &transaction,
                                        const std::shared_ptr<TransactionData> &data) {
    auto transactionData = std::dynamic_pointer_cast<TransactionData>(data);
    if (transactionData->m_tableReference == 0) return;
    transactionData->setStatus(QUERY_COMPLETE);
    LUA->CreateTable();
    int index = 0;
    // Set the correct callback data for the queries of the transaction
    for (auto &pair: transactionData->m_queries) {
        LUA->PushNumber((double) (++index));
        auto query = pair.first;
        //So we get the current data rather than caching it, if the same query is added multiple times.
        LuaQuery::freeDataReference(LUA, *query);
        auto queryData = std::dynamic_pointer_cast<QueryData>(pair.second);
        query->setCallbackData(pair.second);
        int ref = LuaQuery::createDataReference(LUA, *query, *queryData);
        LUA->ReferencePush(ref);
        LUA->SetTable(-3);
        //The last data reference can stay cached in the query and will be freed once the query is gc'ed
    }
    if (!LuaIQuery::pushCallbackReference(LUA, data->m_successReference, data->m_tableReference,
                                          "onSuccess", data->isFirstData())) {
        LUA->Pop(); //Table of results
        return;
    }
    LUA->ReferencePush(transactionData->m_tableReference);
    LUA->Push(-3); //Table of results
    LuaObject::pcallWithErrorReporter(LUA, 2);

    LUA->Pop(); //Table of results

    for (auto &pair: transactionData->m_queries) {
        //We should only cache the data for the duration of the callback
        LuaQuery::freeDataReference(LUA, *pair.first);
    }
}