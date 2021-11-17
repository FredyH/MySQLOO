#include "LuaPreparedQuery.h"

MYSQLOO_LUA_FUNCTION(setNumber) {
    auto luaQuery = LuaObject::getLuaObject<LuaPreparedQuery>(LUA);
    auto query = (PreparedQuery *) luaQuery->m_query.get();
    LUA->CheckType(2, GarrysMod::Lua::Type::Number);
    LUA->CheckType(3, GarrysMod::Lua::Type::Number);
    double index = LUA->GetNumber(2);
    auto uIndex = (unsigned int) index;
    double value = LUA->GetNumber(3);

    query->setNumber(uIndex, value);
    return 0;
}

MYSQLOO_LUA_FUNCTION(setString) {
    auto luaQuery = LuaObject::getLuaObject<LuaPreparedQuery>(LUA);
    auto query = (PreparedQuery *) luaQuery->m_query.get();
    LUA->CheckType(2, GarrysMod::Lua::Type::Number);
    LUA->CheckType(3, GarrysMod::Lua::Type::String);
    double index = LUA->GetNumber(2);
    unsigned int length = 0;
    const char *string = LUA->GetString(3, &length);
    auto uIndex = (unsigned int) index;
    query->setString(uIndex, std::string(string, length));
    return 0;
}

MYSQLOO_LUA_FUNCTION(setBoolean) {
    auto luaQuery = LuaObject::getLuaObject<LuaPreparedQuery>(LUA);
    auto query = (PreparedQuery *) luaQuery->m_query.get();
    LUA->CheckType(2, GarrysMod::Lua::Type::Number);
    LUA->CheckType(3, GarrysMod::Lua::Type::Bool);
    double index = LUA->GetNumber(2);
    auto uIndex = (unsigned int) index;
    bool value = LUA->GetBool(3);
    query->setBoolean(uIndex, value);
    return 0;
}

MYSQLOO_LUA_FUNCTION(setNull) {
    auto luaQuery = LuaObject::getLuaObject<LuaPreparedQuery>(LUA);
    auto query = (PreparedQuery *) luaQuery->m_query.get();
    LUA->CheckType(2, GarrysMod::Lua::Type::Number);
    double index = LUA->GetNumber(2);
    auto uIndex = (unsigned int) index;
    query->setNull(uIndex);
    return 0;
}

MYSQLOO_LUA_FUNCTION(putNewParameters) {
    auto luaQuery = LuaObject::getLuaObject<LuaPreparedQuery>(LUA);
    auto query = (PreparedQuery *) luaQuery->m_query.get();
    query->putNewParameters();
    return 0;
}

MYSQLOO_LUA_FUNCTION(clearParameters) {
    auto luaQuery = LuaObject::getLuaObject<LuaPreparedQuery>(LUA);
    auto query = (PreparedQuery *) luaQuery->m_query.get();
    query->clearParameters();
    return 0;
}

void LuaPreparedQuery::createMetaTable(ILuaBase *LUA) {
    LuaIQuery::TYPE_PREPARED_QUERY = LUA->CreateMetaTable("MySQLOO Prepared Query");
    LuaQuery::addMetaTableFunctions(LUA);

    LUA->PushCFunction(setNumber);
    LUA->SetField(-2, "setNumber");
    LUA->PushCFunction(setString);
    LUA->SetField(-2, "setString");
    LUA->PushCFunction(setBoolean);
    LUA->SetField(-2, "setBoolean");
    LUA->PushCFunction(setNull);
    LUA->SetField(-2, "setNull");
    LUA->PushCFunction(putNewParameters);
    LUA->SetField(-2, "putNewParameters");
    LUA->PushCFunction(clearParameters);
    LUA->SetField(-2, "clearParameters");

    LUA->Pop(); //Metatable
}

std::shared_ptr<IQueryData> LuaPreparedQuery::buildQueryData(ILuaBase* LUA, int stackPosition, bool shouldRef) {
    auto query = (PreparedQuery*) m_query.get();
    auto data = query->buildQueryData();
    if (shouldRef) {
        LuaIQuery::referenceCallbacks(LUA, stackPosition, *data);
    }
    return data;
}
