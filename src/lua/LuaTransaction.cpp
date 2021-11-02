#include "LuaTransaction.h"
#include "LuaQuery.h"
#include "../mysql/Transaction.h"

MYSQLOO_LUA_FUNCTION(addQuery) {
    auto luaTransaction = LuaObject::getLuaObject<LuaTransaction>(LUA, LuaObject::TYPE_TRANSACTION);

    auto addedLuaQuery = LuaQuery::getLuaQuery(LUA, 2);
    auto addedQuery = (Query *) addedLuaQuery->m_query.get();
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

    auto queryData = std::dynamic_pointer_cast<QueryData>(addedLuaQuery->buildQueryData(LUA, 2));

    luaTransaction->m_addedQueryData.push_back(queryData);
    return 0;
}

MYSQLOO_LUA_FUNCTION(getQueries) {
    LUA->GetField(1, "__queries");
    return 1;
}

MYSQLOO_LUA_FUNCTION(clearQueries) {
    auto luaTransaction = LuaObject::getLuaObject<LuaTransaction>(LUA, LuaObject::TYPE_TRANSACTION);

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

std::shared_ptr<IQueryData> LuaTransaction::buildQueryData(ILuaBase *LUA, int stackPosition) {
    LUA->GetField(stackPosition, "__queries");

    std::deque<std::pair<std::shared_ptr<Query>, std::shared_ptr<IQueryData>>> queries;

    for (int i = 0; i < this->m_addedQueryData.size(); i++) {
        auto& queryData = this->m_addedQueryData[i];
        LUA->PushNumber((double) (i + 1));
        LUA->RawGet(-2);
        if (!LUA->IsType(-1, GarrysMod::Lua::Type::Table)) {
            LUA->Pop();
            break;
        }
        auto luaQuery = LuaQuery::getLuaQuery(LUA, -1);
        auto query = std::dynamic_pointer_cast<Query>(luaQuery->m_query);
        query->addQueryData(queryData);
        queries.emplace_back(query, queryData);
    }

    auto data = Transaction::buildQueryData(queries);
    LuaIQuery::referenceCallbacks(LUA, stackPosition, *data);
    return data;
}

void LuaTransaction::runSuccessCallback(ILuaBase *LUA, const std::shared_ptr<IQueryData> &data) {
    auto transactionData = std::dynamic_pointer_cast<TransactionData>(data);
    transactionData->setStatus(QUERY_COMPLETE);
    std::vector<int> queryTableRefs;
    for (auto& pair : transactionData->m_queries) {
        auto query = pair.first;
        //So we get the current data rather than caching it, if the same query is added multiple times.
        query->m_dataReference = 0;
        auto queryData = std::dynamic_pointer_cast<QueryData>(pair.second);
        query->setCallbackData(pair.second);
        int ref = LuaQuery::createDataReference(LUA, *query, *queryData);
        queryTableRefs.push_back(ref);
    }
    LUA->CreateTable();
    for (size_t i = 0; i < queryTableRefs.size(); i++) {
        LUA->PushNumber((double) (i + 1));
        LUA->ReferencePush(queryTableRefs[i]);
        LUA->SetTable(-3);
    }
    int dataRef = LUA->ReferenceCreate();
    if (data->getSuccessReference() != 0) {

    } else if (data->isFirstData()) {

    }

    LUA->ReferenceFree(dataRef);
}
