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

    luaTransaction->m_addedQueryData.push_back(addedQuery->buildQueryData());
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

std::shared_ptr<IQueryData> LuaTransaction::buildQueryData() {
    return std::shared_ptr<IQueryData>();
}
