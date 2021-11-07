
#include "LuaIQuery.h"
#include "../mysql/Query.h"


MYSQLOO_LUA_FUNCTION(start) {
    auto query = LuaIQuery::getLuaObject<LuaIQuery>(LUA);
    auto queryData = query->buildQueryData(LUA, 1);
    query->m_query->start(queryData);
    return 0;
}

MYSQLOO_LUA_FUNCTION(error) {
    auto query = LuaIQuery::getLuaObject<LuaIQuery>(LUA);
    LUA->PushString(query->m_query->error().c_str());
    return 1;
}

MYSQLOO_LUA_FUNCTION(wait) {
    bool shouldSwap = false;
    if (LUA->IsType(2, GarrysMod::Lua::Type::Bool)) {
        shouldSwap = LUA->GetBool(2);
    }
    auto query = LuaIQuery::getLuaObject<LuaIQuery>(LUA);
    query->m_query->wait(shouldSwap);
    return 0;
}

MYSQLOO_LUA_FUNCTION(setOption) {
    auto query = LuaIQuery::getLuaObject<LuaIQuery>(LUA);
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
    auto query = LuaIQuery::getLuaObject<LuaIQuery>(LUA);
    LUA->PushBool(query->m_query->isRunning());
    return 1;
}

MYSQLOO_LUA_FUNCTION(abort) {
    auto query = LuaIQuery::getLuaObject<LuaIQuery>(LUA);
    auto abortedData = query->m_query->abort();
    for (auto &data: abortedData) {
        LuaIQuery::runAbortedCallback(LUA, data);
    }
    return 0;
}

void LuaIQuery::runAbortedCallback(ILuaBase *LUA, const std::shared_ptr<IQueryData> &data) {
    if (data->m_tableReference == 0) return;

    if (!LuaIQuery::pushCallbackReference(LUA, data->m_abortReference, data->m_tableReference,
                                          "onAborted", data->isFirstData())) {
        return;
    }
    LUA->ReferencePush(data->m_tableReference);
    LuaObject::pcallWithErrorReporter(LUA, 1);
}

void LuaIQuery::runErrorCallback(ILuaBase *LUA, const std::shared_ptr<IQueryData> &data) {
    if (data->m_tableReference == 0) return;

    if (!LuaIQuery::pushCallbackReference(LUA, data->m_errorReference, data->m_tableReference,
                                          "onError", data->isFirstData())) {
        return;
    }
    LUA->ReferencePush(data->m_tableReference);
    auto error = data->getError();
    LUA->PushString(error.c_str());
    LuaObject::pcallWithErrorReporter(LUA, 2);
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

void LuaIQuery::referenceCallbacks(ILuaBase *LUA, int stackPosition, IQueryData &data) {
    LUA->Push(stackPosition);
    data.m_tableReference = LUA->ReferenceCreate();

    if (data.m_successReference == 0) {
        data.m_successReference = getFunctionReference(LUA, stackPosition, "onSuccess");
    }

    if (data.m_abortReference == 0) {
        data.m_abortReference = getFunctionReference(LUA, stackPosition, "onAborted");
    }

    if (data.m_onDataReference == 0) {
        data.m_onDataReference = getFunctionReference(LUA, stackPosition, "onData");
    }

    if (data.m_errorReference == 0) {
        data.m_errorReference = getFunctionReference(LUA, stackPosition, "onError");
    }
}

void LuaIQuery::finishQueryData(GarrysMod::Lua::ILuaBase *LUA, const std::shared_ptr<IQueryData> &data) const {
    auto query = this->m_query;
    query->finishQueryData(data);
    if (data->m_tableReference) {
        LUA->ReferenceFree(data->m_tableReference);
    }
    if (data->m_onDataReference) {
        LUA->ReferenceFree(data->m_onDataReference);
    }
    if (data->m_errorReference) {
        LUA->ReferenceFree(data->m_errorReference);
    }
    if (data->m_abortReference) {
        LUA->ReferenceFree(data->m_abortReference);
    }
    if (data->m_successReference) {
        LUA->ReferenceFree(data->m_successReference);
    }
    data->m_onDataReference = 0;
    data->m_errorReference = 0;
    data->m_abortReference = 0;
    data->m_successReference = 0;
    data->m_tableReference = 0;
}

void LuaIQuery::runCallback(ILuaBase *LUA, const std::shared_ptr<IQueryData> &data) {
    m_query->setCallbackData(data);

    auto status = data->getResultStatus();
    switch (status) {
        case QUERY_NONE:
            break; //Should not happen
        case QUERY_ERROR:
            runErrorCallback(LUA, data);
            break;
        case QUERY_SUCCESS:
            runSuccessCallback(LUA, data);
            break;
    }

    finishQueryData(LUA, data);
}

void LuaIQuery::onDestroyedByLua(ILuaBase *LUA) {
    if (m_databaseReference != 0) {
        LUA->ReferenceFree(m_databaseReference);
        m_databaseReference = 0;
    }
}
