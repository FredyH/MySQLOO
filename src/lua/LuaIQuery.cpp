
#include "LuaIQuery.h"
#include "../mysql/Query.h"


MYSQLOO_LUA_FUNCTION(start) {
    auto query = LuaIQuery::getLuaIQuery(LUA);
    auto queryData = query->buildQueryData();
    query->m_query->start(queryData);
    return 0;
}

MYSQLOO_LUA_FUNCTION(error) {
    auto query = LuaIQuery::getLuaIQuery(LUA);
    LUA->PushString(query->m_query->error().c_str());
    return 1;
}

MYSQLOO_LUA_FUNCTION(wait) {
    bool shouldSwap = false;
    if (LUA->IsType(2, GarrysMod::Lua::Type::Bool)) {
        shouldSwap = LUA->GetBool(2);
    }
    auto query = LuaIQuery::getLuaIQuery(LUA);
    query->m_query->wait(shouldSwap);
    return 0;
}

MYSQLOO_LUA_FUNCTION(setOption) {
    auto query = LuaIQuery::getLuaIQuery(LUA);
    LUA->CheckType(2, GarrysMod::Lua::Type::Number);
    bool set = true;
    int option = (int) LUA->GetNumber(2);

    if (LUA->Top() >= 3) {
        LUA->CheckType(3, GarrysMod::Lua::Type::Bool);
        set = LUA->GetBool(3);
    }
    query->m_query->setOption(option, set);
    return 0;
}

MYSQLOO_LUA_FUNCTION(isRunning) {
    auto query = LuaIQuery::getLuaIQuery(LUA);
    LUA->PushBool(query->m_query->isRunning());
    return 1;
}

MYSQLOO_LUA_FUNCTION(abort) {
    auto query = LuaIQuery::getLuaIQuery(LUA);
    auto abortedData = query->m_query->abort();
    for (auto &data: abortedData) {
        LuaIQuery::runAbortedCallback(LUA, data);
    }
    return 0;
}

void LuaIQuery::runAbortedCallback(ILuaBase *LUA, const std::shared_ptr<IQueryData> &data) {
    if (data->m_tableReference == 0) return;

    if (!LuaIQuery::getCallbackReference(LUA, data->m_abortReference, data->m_tableReference,
                                         "onAborted", data->isFirstData())) {
        return;
    }
    LUA->ReferencePush(data->m_tableReference);
    LuaObject::pcallWithErrorReporter(LUA, 1, 0);
}

void LuaIQuery::runErrorCallback(ILuaBase *LUA, const std::shared_ptr<IQueryData> &data) {
    if (data->m_tableReference == 0) return;

    if (!LuaIQuery::getCallbackReference(LUA, data->m_errorReference, data->m_tableReference,
                                         "onError", data->isFirstData())) {
        return;
    }
    LUA->ReferencePush(data->m_tableReference);
    LUA->PushString(data->getError().c_str());
    LuaObject::pcallWithErrorReporter(LUA, 2, 0);
}

void LuaIQuery::addMetaTableFunctions(ILuaBase *LUA) {
    LuaObject::addMetaTableFunctions(LUA);

    LUA->PushCFunction(start);
    LUA->SetField(-2, "start");
    LUA->PushCFunction(error);
    LUA->SetField(-2, "error");
    LUA->PushCFunction(wait);
    LUA->SetField(-2, "wait");
    LUA->PushCFunction(setOption);
    LUA->SetField(-2, "setOption");
    LUA->PushCFunction(isRunning);
    LUA->SetField(-2, "isRunning");
    LUA->PushCFunction(abort);
    LUA->SetField(-2, "abort");
}
