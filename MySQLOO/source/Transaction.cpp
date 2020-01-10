#include "Transaction.h"
#include "ResultData.h"
#include "errmsg.h"
#include "Database.h"

Transaction::Transaction(Database* dbase, GarrysMod::Lua::ILuaBase* LUA) : IQuery(dbase, LUA) {
	registerFunction(LUA, "addQuery", Transaction::addQuery);
	registerFunction(LUA, "getQueries", Transaction::getQueries);
	registerFunction(LUA, "clearQueries", Transaction::clearQueries);
}

void Transaction::onDestroyed(GarrysMod::Lua::ILuaBase* LUA) {}

//TODO Fix memory leak if transaction is never started
int Transaction::addQuery(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Transaction* transaction = dynamic_cast<Transaction*>(unpackSelf(LUA, TYPE_QUERY));
	if (transaction == nullptr) {
		LUA->ThrowError("Tried to pass wrong self");
	}
	IQuery* iQuery = (IQuery*)unpackLuaObject(LUA, 2, TYPE_QUERY, false);
	Query* query = dynamic_cast<Query*>(iQuery);
	if (query == nullptr) {
		LUA->ThrowError("Tried to pass non query to addQuery()");
	}
	//This is all very ugly
	LUA->Push(1);
	LUA->GetField(-1, "__queries");
	if (LUA->IsType(-1, GarrysMod::Lua::Type::NIL)) {
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
	LUA->Push(4);
	return 0;
}

int Transaction::getQueries(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Transaction* transaction = dynamic_cast<Transaction*>(unpackSelf(LUA, TYPE_QUERY));
	if (transaction == nullptr) {
		LUA->ThrowError("Tried to pass wrong self");
	}
	LUA->Push(1);
	LUA->GetField(-1, "__queries");
	return 1;
}

int Transaction::clearQueries(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Transaction* transaction = dynamic_cast<Transaction*>(unpackSelf(LUA, TYPE_QUERY));
	if (transaction == nullptr) {
		LUA->ThrowError("Tried to pass wrong self");
	}
	LUA->Push(1);
	LUA->PushNil();
	LUA->SetField(-2, "__queries");
	LUA->Pop();
	return 0;
}

//Calls the lua callbacks associated with this query
void Transaction::doCallback(GarrysMod::Lua::ILuaBase* LUA, std::shared_ptr<IQueryData> ptr) {
	TransactionData* data = (TransactionData*)ptr.get();
	data->setStatus(QUERY_COMPLETE);
	for (auto& pair : data->m_queries) {
		auto query = pair.first;
		auto queryData = pair.second;
		query->setCallbackData(queryData);
	}
	switch (data->getResultStatus()) {
	case QUERY_NONE:
		break;
	case QUERY_ERROR:
		if (data->getErrorReference() != 0) {
			this->runFunction(LUA, data->getErrorReference(), "s", data->getError().c_str());
		} else if (data->isFirstData()) {
			this->runCallback(LUA, "onError", "s", data->getError().c_str());
		}
		break;
	case QUERY_SUCCESS:
		if (data->getSuccessReference() != 0) {
			this->runFunction(LUA, data->getSuccessReference());
		} else if (data->isFirstData()) {
			this->runCallback(LUA, "onSuccess");
		}
		break;
	}
	for (auto& pair : data->m_queries) {
		auto query = pair.first;
		auto queryData = pair.second;
		query->onQueryDataFinished(LUA, queryData);
	}
}

bool Transaction::executeStatement(MYSQL* connection, std::shared_ptr<IQueryData> ptr) {
	TransactionData* data = (TransactionData*)ptr.get();
	data->setStatus(QUERY_RUNNING);
	//This temporarily disables reconnect, since a reconnect
	//would rollback (and cancel) a transaction
	//Which could lead to parts of the transaction being executed outside of a transaction
	//If they are being executed after the reconnect
	my_bool oldReconnectStatus = m_database->getAutoReconnect();
	m_database->setAutoReconnect((my_bool) 0);
	auto resetReconnectStatus = finally([&] { m_database->setAutoReconnect(oldReconnectStatus); });
	try {
		this->mysqlAutocommit(connection, false);
		{
			for (auto& query : data->m_queries) {
				try {
					//Errors are cleared in case this is retrying after losing connection
					query.second->setResultStatus(QUERY_NONE);
					query.second->setError("");
					query.first->executeQuery(connection, query.second);
				} catch (const MySQLException& error) {
					query.second->setError(error.what());
					query.second->setResultStatus(QUERY_ERROR);
					throw error;
				}
			}
		}
		mysql_commit(connection);
		data->setResultStatus(QUERY_SUCCESS);
		this->mysqlAutocommit(connection, true);
	} catch (const MySQLException& error) {
		//This check makes sure that setting mysqlAutocommit back to true doesn't cause the transaction to fail
		//Even though the transaction was executed successfully
		if (data->getResultStatus() != QUERY_SUCCESS) {
			int errorCode = error.getErrorCode();
			if (oldReconnectStatus && !data->retried &&
				(errorCode == CR_SERVER_LOST || errorCode == CR_SERVER_GONE_ERROR)) {
				//Because autoreconnect is disabled we want to try and explicitly execute the transaction once more
				//if we can get the client to reconnect (reconnect is caused by mysql_ping)
				//If this fails we just go ahead and error
				m_database->setAutoReconnect((my_bool) 1);
				if (mysql_ping(connection) == 0) {
					data->retried = true;
					return executeStatement(connection, ptr);
				}
			}
			//If this call fails it means that the connection was (probably) lost
			//In that case the mysql server rolls back any transaction anyways so it doesn't
			//matter if it fails
			mysql_rollback(connection);
			data->setResultStatus(QUERY_ERROR);
		}
		//If this fails it probably means that the connection was lost
		//In that case autocommit is turned back on anyways (once the connection is reestablished)
		//See: https://dev.mysql.com/doc/refman/5.7/en/auto-reconnect.html
		mysql_autocommit(connection, true);
		data->setError(error.what());
	}
	for (auto& pair : data->m_queries) {
		pair.second->setResultStatus(data->getResultStatus());
		pair.second->setStatus(QUERY_COMPLETE);
	}
	data->setStatus(QUERY_COMPLETE);
	return true;
}


std::shared_ptr<IQueryData> Transaction::buildQueryData(GarrysMod::Lua::ILuaBase* LUA) {
	//At this point the transaction is guaranteed to have a referenced table
	//since this is always called shortly after transaction:start()
	std::shared_ptr<IQueryData> ptr(new TransactionData());
	TransactionData* data = (TransactionData*)ptr.get();
	this->pushTableReference(LUA);
	LUA->GetField(-1, "__queries");
	if (!LUA->IsType(-1, GarrysMod::Lua::Type::TABLE)) {
		LUA->Pop(2);
		return ptr;
	}
	int index = 1;
	//Stuff could go horribly wrong here if a lua error occurs
	//but it really shouldn't
	while (true) {
		LUA->PushNumber(index++);
		LUA->GetTable(-2);
		if (!LUA->IsType(-1, GarrysMod::Lua::Type::TABLE)) {
			LUA->Pop();
			break;
		}
		//This would error if it's not a query
		Query* iQuery = (Query*)unpackLuaObject(LUA, -1, TYPE_QUERY, false);
		auto queryPtr = std::dynamic_pointer_cast<Query>(iQuery->getSharedPointerInstance());
		auto queryData = iQuery->buildQueryData(LUA);
		iQuery->addQueryData(LUA, queryData, false);
		data->m_queries.push_back(std::make_pair(queryPtr, queryData));
		LUA->Pop();
	}
	LUA->Pop(2);
	return ptr;
}