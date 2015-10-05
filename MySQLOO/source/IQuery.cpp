#include "IQuery.h"
#include "Database.h"
#include "ResultData.h"
#include "Logger.h"

//Important:
//Calling any query functions that rely on data from the query thread
//before the callback is called can result in race conditions.
//Always check for QUERY_COMPLETE!!!

IQuery::IQuery(Database* dbase, lua_State* state) : LuaObjectBase(state, TYPE_QUERY), m_database(dbase)
{
	LOG_CURRENT_FUNCTIONCALL
	m_options = OPTION_NAMED_FIELDS | OPTION_INTERPRET_DATA | OPTION_CACHE;
	m_status = QUERY_NOT_RUNNING;
	registerFunction(state, "start", IQuery::start);
	registerFunction(state, "lastInsert", IQuery::lastInsert);
	registerFunction(state, "getData", IQuery::getData_Wrapper);
	registerFunction(state, "error", IQuery::error);
	registerFunction(state, "wait", IQuery::wait);
	registerFunction(state, "setOption", IQuery::setOption);
	registerFunction(state, "isRunning", IQuery::isRunning);
	registerFunction(state, "abort", IQuery::abort);
	registerFunction(state, "hasMoreResults", IQuery::hasMoreResults);
	registerFunction(state, "getNextResults", IQuery::getNextResults);
}

IQuery::~IQuery()
{
}

//When the query is destroyed by lua
void IQuery::onDestroyed(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	if (this->dataReference != 0 && state != NULL)
	{
		//Make sure data associated with this query can be freed as well
		LUA->ReferenceFree(this->dataReference);
		this->dataReference = 0;
	}
}

//Sets the mysql query string
void IQuery::setQuery(std::string query)
{
	m_query = query;
}

//This function just returns the data associated with the query
//Data is only created once (and then the reference to that data is returned)
int IQuery::getData_Wrapper(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	IQuery* object = (IQuery*)unpackSelf(state, TYPE_QUERY);
	if (object->m_resultStatus != QUERY_SUCCESS)
	{
		LUA->PushNil();
	}
	else
	{
		LUA->ReferencePush(object->getData(state));
	}
	return 1;
}

//Returns true if a query has at least one additional ResultSet left
int IQuery::hasMoreResults(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	IQuery* object = (IQuery*)unpackSelf(state, TYPE_QUERY);
	if (object->m_status != QUERY_COMPLETE)
	{
		LUA->ThrowError("Query not completed yet");
	}
	LUA->PushBool(object->results.size() > 0);
	return 1;
}

//Unreferences the current result set and uses the next result set
int IQuery::getNextResults(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	IQuery* object = (IQuery*)unpackSelf(state, TYPE_QUERY);
	if (object->m_status != QUERY_COMPLETE)
	{
		LUA->ThrowError("Query not completed yet");
	}
	else if (object->results.size() == 0)
	{
		LUA->ThrowError("Query doesn't have any more results");
	}
	if (object->dataReference != 0)
	{
		LUA->ReferenceFree(object->dataReference);
		object->dataReference = 0;
	}
	if (object->insertIds.size() > 0)
	{
		object->insertIds.pop_front();
	}
	LUA->ReferencePush(object->getData(state));
	return 1;
}

//Stores the data associated with the current result set of the query
//Only called once per result set (and then cached)
int IQuery::getData(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	if (this->dataReference != 0)
		return this->dataReference;
	LUA->CreateTable();
	if (this->results.size() > 0)
	{
		ResultData& currentData = this->results.front();
		for (unsigned int i = 0; i < currentData.getRows().size(); i++)
		{
			ResultDataRow& row = currentData.getRows()[i];
			LUA->CreateTable();
			int rowObject = LUA->ReferenceCreate();
			for (unsigned int j = 0; j < row.getValues().size(); j++)
			{
				dataToLua(state, rowObject, j + 1, row.getValues()[j], currentData.getColumns()[j].c_str(), currentData.getColumnTypes()[j], row.isFieldNull(j));
			}
			LUA->PushNumber(i+1);
			LUA->ReferencePush(rowObject);
			LUA->SetTable(-3);
			LUA->ReferenceFree(rowObject);
		}
		//ResultSet is not needed anymore since we stored it in lua tables.
		this->results.pop_front();
	}
	this->dataReference = LUA->ReferenceCreate();
	return this->dataReference;
};


//Function that converts the data stored in a mysql field into a lua type
void IQuery::dataToLua(lua_State* state, int rowReference, unsigned int column, std::string &columnValue, const char* columnName, int columnType, bool isNull)
{
	LUA->ReferencePush(rowReference);
	if (this->m_options & OPTION_NUMERIC_FIELDS)
	{
		LUA->PushNumber(column);
	}
	if (isNull)
	{
		LUA->PushNil();
	}
	else
	{
		switch (columnType)
		{
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
			LUA->PushString(columnValue.c_str(), columnValue.length());
			break;
		}
	}
	if (this->m_options & OPTION_NUMERIC_FIELDS)
	{
		LUA->SetTable(-3);
	}
	else
	{
		LUA->SetField(-2, columnName);
	}
	LUA->Pop();
}

//Queues the query into the queue of the database instance associated with it
int IQuery::start(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	IQuery* object = (IQuery*)unpackSelf(state, TYPE_QUERY, true);
	if (object->m_status != QUERY_NONE)
	{
		LUA->ThrowError("Query already started");
	}
	else
	{
		object->m_database->enqueueQuery(object);
	}
	return 0;
}

//Returns if the query has been queued with the database instance
int IQuery::isRunning(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	IQuery* object = (IQuery*)unpackSelf(state, TYPE_QUERY);
	LUA->PushBool(object->m_status == QUERY_RUNNING || object->m_status == QUERY_WAITING);
	return 1;
}

//Returns the last insert id produced by INSERT INTO statements (or 0 if there is none)
int IQuery::lastInsert(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	IQuery* object = (IQuery*)unpackSelf(state, TYPE_QUERY);
	//Calling lastInsert() after query was executed but before the callback is run can cause race conditions
	if (object->m_status != QUERY_COMPLETE || object->insertIds.size() == 0)
		LUA->PushNumber(0);
	else
		LUA->PushNumber(object->insertIds.front());
	return 1;
}

//Blocks the current Thread until the query has finished processing
//Possibly dangerous (dead lock when database goes down while waiting)
int IQuery::wait(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	IQuery* object = (IQuery*)unpackSelf(state, TYPE_QUERY);
	if (object->m_status == QUERY_NOT_RUNNING)
	{
		LUA->ThrowError("Query not started.");
	}
	{
		std::lock_guard<std::mutex> lck(object->m_database->m_queryQueueMutex);
		auto pos = std::find_if(object->m_database->queryQueue.begin(), object->m_database->queryQueue.end(), [&](std::shared_ptr<IQuery> const& p) {
			return p.get() == object;
		});
		if (pos != object->m_database->queryQueue.begin() && pos != object->m_database->queryQueue.end())
		{
			std::iter_swap(pos, object->m_database->queryQueue.begin());
		}
	}
	{
		std::unique_lock<std::mutex> lck(object->m_database->m_finishedQueueMutex);
		while (!object->finished) object->m_waitWakeupVariable.wait(lck);
	}
	object->m_database->think(state);
	return 0;
}

//Returns the error message produced by the mysql query or 0 if there is none
int IQuery::error(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	IQuery* object = (IQuery*)unpackSelf(state, TYPE_QUERY);
	if (object->m_status != QUERY_COMPLETE) 
		return 0;
	else
	{
		LUA->PushString(object->m_errorText.c_str());
		return 1;
	}
}

//Attempts to abort the query, returns true if it was able to stop the query in time, false otherwise
int IQuery::abort(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	IQuery* object = (IQuery*)unpackSelf(state, TYPE_QUERY);
	std::lock_guard<std::mutex> lock(object->m_database->m_queryQueueMutex);
	auto it = std::find_if(object->m_database->queryQueue.begin(), object->m_database->queryQueue.end(), [&](std::shared_ptr<IQuery> const& p) {
		return p.get() == object;
	});
	if (it != object->m_database->queryQueue.end())
	{
		object->m_database->queryQueue.erase(it);
		object->m_status = QUERY_ABORTED;
		object->unreference(state);
		LUA->PushBool(true);
	}
	else
	{
		LUA->PushBool(false);
	}
	return 1;
}

//Sets several query options
int IQuery::setOption(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	IQuery* object = (IQuery*)unpackSelf(state, TYPE_QUERY);
	LUA->CheckType(2, GarrysMod::Lua::Type::NUMBER);
	bool set = true;
	int option = LUA->GetNumber(2);
	if (option != OPTION_NUMERIC_FIELDS &&
		option != OPTION_NAMED_FIELDS &&
		option != OPTION_INTERPRET_DATA &&
		option != OPTION_CACHE)
	{
		LUA->ThrowError("Invalid option");
		return 0;
	}

	if (LUA->Top() >= 3)
	{
		LUA->CheckType(3, GarrysMod::Lua::Type::BOOL);
		set = LUA->GetBool(3);
	}

	if (set)
	{
		object->m_options |= option;
	}
	else
	{
		object->m_options &= ~option;
	}
	return 0;
}

//Calls the lua callbacks associated with this query
void IQuery::doCallback(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	this->m_status = QUERY_COMPLETE;

	switch (this->m_resultStatus)
	{
	case QUERY_NONE:
		break;
	case QUERY_ERROR:
		this->runCallback(state, "onError", "ss", this->m_errorText.c_str(), this->m_query.c_str());
		break;
	case QUERY_SUCCESS:
		int dataref = this->getData(state);
		if (this->hasCallback(state, "onData"))
		{
			LUA->ReferencePush(dataref);
			LUA->PushNil();
			while (LUA->Next(-2))
			{
				//Top is now the row, top-1 row index
				int rowReference = LUA->ReferenceCreate();
				this->runCallback(state, "onData", "r", rowReference);
				LUA->ReferenceFree(rowReference);
				//Don't have to pop since reference create consumed the value
			}
			LUA->Pop();
		}
		this->runCallback(state, "onSuccess", "r", dataref);
		break;
	}
}

//Wrapper for c api calls
//Just throws an exception if anything goes wrong for ease of use

void IQuery::mysqlQuery(MYSQL* sql, std::string &query)
{
	LOG_CURRENT_FUNCTIONCALL
	int result = mysql_real_query(sql, query.c_str(), query.length());
	if (result != 0)
	{
		const char* errorMessage = mysql_error(sql);
		int errorCode = mysql_errno(sql);
		throw MySQLException(errorCode, errorMessage);
	}
}

MYSQL_RES* IQuery::mysqlStoreResults(MYSQL* sql)
{
	MYSQL_RES* result = mysql_store_result(sql);
	if (result == NULL)
	{
		int errorCode = mysql_errno(sql);
		if (errorCode != 0)
		{
			const char* errorMessage = mysql_error(sql);
			throw MySQLException(errorCode, errorMessage);
		}
	}
	return result;
}

bool IQuery::mysqlNextResult(MYSQL* sql)
{
	int result = mysql_next_result(sql);
	if (result == 0) return true;
	if (result == -1) return false;
	int errorCode = mysql_errno(sql);
	if (errorCode != 0)
	{
		const char* errorMessage = mysql_error(sql);
		throw MySQLException(errorCode, errorMessage);
	}
	return false;
}