#include "IQuery.h"
#include "Database.h"
#include "ResultData.h"

//Important:
//Calling any query functions that rely on data from the query thread
//before the callback is called can result in race conditions.
//Always check for QUERY_COMPLETE!!!

IQuery::IQuery(Database* dbase, lua_State* state) : LuaObjectBase(state, false, TYPE_QUERY), m_database(dbase) {
	m_options = OPTION_NAMED_FIELDS | OPTION_INTERPRET_DATA | OPTION_CACHE;
	registerFunction(state, "start", IQuery::start);
	registerFunction(state, "error", IQuery::error);
	registerFunction(state, "wait", IQuery::wait);
	registerFunction(state, "setOption", IQuery::setOption);
	registerFunction(state, "isRunning", IQuery::isRunning);
	registerFunction(state, "abort", IQuery::abort);
}

IQuery::~IQuery() {}

QueryResultStatus IQuery::getResultStatus() {
	if (!hasCallbackData()) {
		return QUERY_NONE;
	}
	return callbackQueryData->getResultStatus();
}

//Wrapper for c api calls
//Just throws an exception if anything goes wrong for ease of use

void IQuery::mysqlAutocommit(MYSQL* sql, bool auto_mode) {
	int result = mysql_autocommit(sql, auto_mode);
	if (result != 0) {
		const char* errorMessage = mysql_error(sql);
		int errorCode = mysql_errno(sql);
		throw MySQLException(errorCode, errorMessage);
	}
}

//Queues the query into the queue of the database instance associated with it
int IQuery::start(lua_State* state) {
	IQuery* object = (IQuery*)unpackSelf(state, TYPE_QUERY);
	if (object->m_database->disconnected) {
		LUA->ThrowError("Database already disconnected.");
	}
	if (object->runningQueryData.size() == 0) {
		referenceTable(state, object, 1);
	}
	std::shared_ptr<IQueryData> ptr = object->buildQueryData(state);
	object->addQueryData(state, ptr);
	object->m_database->enqueueQuery(object, ptr);
	object->hasBeenStarted = true;
	return 0;
}

//Returns if the query has been queued with the database instance
int IQuery::isRunning(lua_State* state) {
	IQuery* object = (IQuery*)unpackSelf(state, TYPE_QUERY);
	LUA->PushBool(object->runningQueryData.size() > 0);
	return 1;
}

//Blocks the current Thread until the query has finished processing
//Possibly dangerous (dead lock when database goes down while waiting)
//If the second argument is set to true, the query is going to be swapped to the front of the query queue
int IQuery::wait(lua_State* state) {
	IQuery* object = (IQuery*)unpackSelf(state, TYPE_QUERY);
	bool shouldSwap = false;
	if (LUA->IsType(2, GarrysMod::Lua::Type::BOOL)) {
		shouldSwap = LUA->GetBool(2);
	}
	if (object->runningQueryData.size() == 0) {
		LUA->ThrowError("Query not started.");
	}
	std::shared_ptr<IQueryData> lastInsertedQuery = object->runningQueryData.back();
	//Changing the order of the query might have unwanted side effects, so this is disabled by default
	if (shouldSwap) {
		object->m_database->queryQueue.swapToFrontIf([&](std::pair<std::shared_ptr<IQuery>, std::shared_ptr<IQueryData>> const& p) {
			return p.second.get() == lastInsertedQuery.get();
		});
	}
	{
		std::unique_lock<std::mutex> lck(object->m_waitMutex);
		while (!lastInsertedQuery->isFinished()) object->m_waitWakeupVariable.wait(lck);
	}
	object->m_database->think(state);
	return 0;
}

//Returns the error message produced by the mysql query or 0 if there is none
int IQuery::error(lua_State* state) {
	IQuery* object = (IQuery*)unpackSelf(state, TYPE_QUERY);
	if (!object->hasCallbackData()) {
		return 0;
	}
	//Calling affectedRows() after query was executed but before the callback is run can cause race conditions
	LUA->PushString(object->callbackQueryData->getError().c_str());
	return 1;
}

//Attempts to abort the query, returns true if it was able to stop at least one query in time, false otherwise
int IQuery::abort(lua_State* state) {
	IQuery* object = (IQuery*)unpackSelf(state, TYPE_QUERY);
	bool wasAborted = false;
	//This is copied so that I can remove entries from that vector in onQueryDataFinished
	auto vec = object->runningQueryData;
	for (auto& data : vec) {
		//It doesn't really matter if any of them are in a transaction since in that case they
		//aren't in the query queue
		bool wasRemoved = object->m_database->queryQueue.removeIf([&](std::pair<std::shared_ptr<IQuery>, std::shared_ptr<IQueryData>> const& p) {
			return p.second.get() == data.get();
		});
		if (wasRemoved) {
			data->setStatus(QUERY_ABORTED);
			wasAborted = true;
			if (data->getAbortReference() != 0) {
				object->runFunction(state, data->getAbortReference());
			} else if (data->isFirstData()) {
				object->runCallback(state, "onAborted");
			}
			object->onQueryDataFinished(state, data);
		}
	}
	LUA->PushBool(wasAborted);
	return 1;
}

//Sets several query options
int IQuery::setOption(lua_State* state) {
	IQuery* object = (IQuery*)unpackSelf(state, TYPE_QUERY);
	LUA->CheckType(2, GarrysMod::Lua::Type::NUMBER);
	bool set = true;
	int option = (int)LUA->GetNumber(2);
	if (option != OPTION_NUMERIC_FIELDS &&
		option != OPTION_NAMED_FIELDS &&
		option != OPTION_INTERPRET_DATA &&
		option != OPTION_CACHE) {
		LUA->ThrowError("Invalid option");
		return 0;
	}

	if (LUA->Top() >= 3) {
		LUA->CheckType(3, GarrysMod::Lua::Type::BOOL);
		set = LUA->GetBool(3);
	}

	if (set) {
		object->m_options |= option;
	} else {
		object->m_options &= ~option;
	}
	return 0;
}

//Wrapper for c api calls
//Just throws an exception if anything goes wrong for ease of use

void IQuery::mysqlQuery(MYSQL* sql, std::string &query) {
	int result = mysql_real_query(sql, query.c_str(), query.length());
	if (result != 0) {
		const char* errorMessage = mysql_error(sql);
		int errorCode = mysql_errno(sql);
		throw MySQLException(errorCode, errorMessage);
	}
}

MYSQL_RES* IQuery::mysqlStoreResults(MYSQL* sql) {
	MYSQL_RES* result = mysql_store_result(sql);
	if (result == nullptr) {
		int errorCode = mysql_errno(sql);
		if (errorCode != 0) {
			const char* errorMessage = mysql_error(sql);
			throw MySQLException(errorCode, errorMessage);
		}
	}
	return result;
}

bool IQuery::mysqlNextResult(MYSQL* sql) {
	int result = mysql_next_result(sql);
	if (result == 0) return true;
	if (result == -1) return false;
	int errorCode = mysql_errno(sql);
	if (errorCode != 0) {
		const char* errorMessage = mysql_error(sql);
		throw MySQLException(errorCode, errorMessage);
	}
	return false;
}

void IQuery::addQueryData(lua_State* state, std::shared_ptr<IQueryData> data, bool shouldRefCallbacks) {
	if (!hasBeenStarted) {
		data->m_wasFirstData = true;
	}
	runningQueryData.push_back(data);
	if (shouldRefCallbacks) {
		data->m_onDataReference = this->getCallbackReference(state, "onData");
		data->m_errorReference = this->getCallbackReference(state, "onError");
		data->m_abortReference = this->getCallbackReference(state, "onAborted");
		data->m_successReference = this->getCallbackReference(state, "onSuccess");
	}
}
void IQuery::onQueryDataFinished(lua_State* state, std::shared_ptr<IQueryData> data) {
	runningQueryData.erase(std::remove(runningQueryData.begin(), runningQueryData.end(), data));
	if (runningQueryData.size() == 0) {
		canbedestroyed = true;
		unreference(state);
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
}