
#include "LuaIQuery.h"
#include "LuaQuery.h"
#include "LuaTransaction.h"
#include "LuaDatabase.h"


MYSQLOO_LUA_FUNCTION(start) {
    auto query = LuaIQuery::getLuaObject<LuaIQuery>(LUA);
    auto queryData = query->buildQueryData(LUA, 1, true);
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
    if (query->m_databaseReference != 0) {
        LUA->ReferencePush(query->m_databaseReference);
        auto database = LuaObject::getLuaObject<LuaDatabase>(LUA, -1);
        database->think(LUA);
        LUA->Pop();
    }

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
        if (auto transaction = std::dynamic_pointer_cast<Transaction>(query->m_query)) {
            LuaTransaction::runAbortedCallback(LUA, transaction, std::dynamic_pointer_cast<TransactionData>(data));
        } else {
            LuaIQuery::runAbortedCallback(LUA, data);
        }
        data->finishLuaQueryData(LUA, query->m_query);
    }
    LUA->PushBool(!abortedData.empty());
    return 1;
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

void LuaIQuery::runErrorCallback(ILuaBase *LUA, const std::shared_ptr<IQuery> &iQuery,
                                 const std::shared_ptr<IQueryData> &data) {
    if (data->m_tableReference == 0) return;

    if (!LuaIQuery::pushCallbackReference(LUA, data->m_errorReference, data->m_tableReference,
                                          "onError", data->isFirstData())) {
        return;
    }
    LUA->ReferencePush(data->m_tableReference);
    auto error = data->getError();
    LUA->PushString(error.c_str());
    LUA->PushString(iQuery->getSQLString().c_str());
    LuaObject::pcallWithErrorReporter(LUA, 3);
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
    data.m_tableReference = LuaReferenceCreate(LUA);

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

void
LuaIQuery::runCallback(ILuaBase *LUA, const std::shared_ptr<IQuery> &iQuery, const std::shared_ptr<IQueryData> &data) {
    iQuery->setCallbackData(data);

    auto status = data->getResultStatus();
    switch (status) {
        case QUERY_NONE:
            break; //Should not happen
        case QUERY_ERROR:
            if (auto transaction = std::dynamic_pointer_cast<Transaction>(iQuery)) {
                LuaTransaction::runErrorCallback(LUA, transaction, std::dynamic_pointer_cast<TransactionData>(data));
            } else {
                LuaIQuery::runErrorCallback(LUA, iQuery, data);
            }
            break;
        case QUERY_SUCCESS:
            if (auto query = std::dynamic_pointer_cast<Query>(iQuery)) {
                LuaQuery::runSuccessCallback(LUA, query, std::dynamic_pointer_cast<QueryData>(data));
            } else if (auto transaction = std::dynamic_pointer_cast<Transaction>(iQuery)) {
                LuaTransaction::runSuccessCallback(LUA, transaction, std::dynamic_pointer_cast<TransactionData>(data));
            }
            break;
    }

    data->finishLuaQueryData(LUA, iQuery);
}

void LuaIQuery::onDestroyedByLua(ILuaBase *LUA) {
    if (m_databaseReference != 0) {
        LuaReferenceFree(LUA, m_databaseReference);
        m_databaseReference = 0;
    }
}
