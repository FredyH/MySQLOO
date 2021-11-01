
#include "LuaQuery.h"

//Function that converts the data stored in a mysql field into a lua type
//Expects the row table to be at the top of the stack at the start of this function
//Adds a column to the row table
static void dataToLua(Query &query,
                      GarrysMod::Lua::ILuaBase *LUA, unsigned int column,
                      std::string &columnValue, const char *columnName, int columnType, bool isNull) {
    if (query.hasOption(OPTION_NUMERIC_FIELDS)) {
        LUA->PushNumber(column);
    }
    if (isNull) {
        LUA->PushNil();
    } else {
        switch (columnType) {
            case MYSQL_TYPE_FLOAT:
            case MYSQL_TYPE_DOUBLE:
            case MYSQL_TYPE_LONGLONG:
            case MYSQL_TYPE_LONG:
            case MYSQL_TYPE_INT24:
            case MYSQL_TYPE_TINY:
            case MYSQL_TYPE_SHORT:
                LUA->PushNumber(atof(columnValue.c_str()));
                break;
            case MYSQL_TYPE_BIT:
                LUA->PushNumber(static_cast<int>(columnValue[0]));
                break;
            case MYSQL_TYPE_NULL:
                LUA->PushNil();
                break;
            default:
                LUA->PushString(columnValue.c_str(), (unsigned int) columnValue.length());
                break;
        }
    }
    if (query.hasOption(OPTION_NUMERIC_FIELDS)) {
        LUA->SetTable(-3);
    } else {
        LUA->SetField(-2, columnName);
    }
    LUA->Pop();
}

//Stores the data associated with the current result set of the query
//Only called once per result set (and then cached)
int LuaQuery::createDataReference(GarrysMod::Lua::ILuaBase *LUA, QueryData &data) {
    auto query = std::dynamic_pointer_cast<Query>(m_query);
    if (query->m_dataReference != 0)
        return query->m_dataReference;
    LUA->CreateTable();
    int dataStackPosition = LUA->Top();
    if (query->hasCallbackData() && data.hasMoreResults()) {
        ResultData &currentData = data.getResult();
        for (unsigned int i = 0; i < currentData.getRows().size(); i++) {
            ResultDataRow &row = currentData.getRows()[i];
            LUA->CreateTable();
            int rowStackPosition = LUA->Top();
            for (unsigned int j = 0; j < row.getValues().size(); j++) {
                dataToLua(*query, LUA, j + 1, row.getValues()[j], currentData.getColumns()[j].c_str(),
                          currentData.getColumnTypes()[j], row.isFieldNull(j));
            }
            LUA->Push(dataStackPosition);
            LUA->PushNumber(i + 1);
            LUA->Push(rowStackPosition);
            LUA->SetTable(-3);
            LUA->Pop(2); //data + row
        }
    }
    query->m_dataReference = LUA->ReferenceCreate();
    return query->m_dataReference;
};

void LuaQuery::runSuccessCallback(ILuaBase *LUA, QueryData &data) {
    int tableReference = this->createDataReference(LUA, data);

    if (!LuaIQuery::getCallbackReference(LUA, data.m_successReference, data.m_tableReference,
                                         "onSuccess", data.isFirstData())) {
        return;
    }
    LUA->ReferencePush(data.m_tableReference);
    LUA->ReferencePush(tableReference);
    LuaObject::pcallWithErrorReporter(LUA, 2, 0);
}

MYSQLOO_LUA_FUNCTION(affectedRows) {
    auto luaQuery = LuaQuery::getLuaQuery(LUA);
    auto query = (Query *) luaQuery->m_query.get();
    LUA->PushNumber((double) query->affectedRows());
    return 1;
}

MYSQLOO_LUA_FUNCTION(lastInsert) {
    auto luaQuery = LuaQuery::getLuaQuery(LUA);
    auto query = (Query *) luaQuery->m_query.get();
    LUA->PushNumber((double) query->lastInsert());
    return 1;
}

MYSQLOO_LUA_FUNCTION(getData) {
    auto luaQuery = LuaQuery::getLuaQuery(LUA);
    auto query = (Query *) luaQuery->m_query.get();
    if (!query->hasCallbackData() || query->callbackQueryData->getResultStatus() == QUERY_ERROR) {
        LUA->PushNil();
    } else {
        int ref = luaQuery->createDataReference(LUA, (QueryData &) *(query->callbackQueryData));
        LUA->ReferencePush(ref);
    }
    return 1;
}

MYSQLOO_LUA_FUNCTION(hasMoreResults) {
    auto luaQuery = LuaQuery::getLuaQuery(LUA);
    auto query = (Query *) luaQuery->m_query.get();
    LUA->PushBool(query->hasMoreResults());
    return 1;
}

LUA_FUNCTION(getNextResults) {
    auto luaQuery = LuaQuery::getLuaQuery(LUA);
    auto query = (Query *) luaQuery->m_query.get();
    query->getNextResults();
    return 0;
}

void LuaQuery::addMetaTableFunctions(ILuaBase *LUA) {
    LuaIQuery::addMetaTableFunctions(LUA);

    LUA->PushCFunction(affectedRows);
    LUA->SetField(-2, "affectedRows");
    LUA->PushCFunction(lastInsert);
    LUA->SetField(-2, "lastInsert");
    LUA->PushCFunction(getData);
    LUA->SetField(-2, "getData");
    LUA->PushCFunction(hasMoreResults);
    LUA->SetField(-2, "hasMoreResults");
    LUA->PushCFunction(getNextResults);
    LUA->SetField(-2, "getNextResults");
}

void LuaQuery::createMetaTable(ILuaBase *LUA) {
    LuaIQuery::TYPE_QUERY = LUA->CreateMetaTable("MySQLOO Query");
    LuaQuery::addMetaTableFunctions(LUA);
    LUA->Pop(); //Metatable
}