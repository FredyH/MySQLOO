#include "Query.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#ifdef LINUX
#include <stdlib.h>
#endif

Query::Query(Database* dbase, lua_State* state) : IQuery(dbase, state) {
	classname = "Query";
	registerFunction(state, "affectedRows", Query::affectedRows);
	registerFunction(state, "lastInsert", Query::lastInsert);
	registerFunction(state, "getData", Query::getData_Wrapper);
	registerFunction(state, "hasMoreResults", Query::hasMoreResults);
	registerFunction(state, "getNextResults", Query::getNextResults);
}

Query::~Query(void) {}

//Calls the lua callbacks associated with this query
void Query::doCallback(lua_State* state, std::shared_ptr<IQueryData> data) {
	if (this->dataReference != 0) {
		LUA->ReferenceFree(this->dataReference);
		this->dataReference = 0;
	}
	switch (data->getResultStatus()) {
	case QUERY_NONE:
		break;
	case QUERY_ERROR:
		if (data->getErrorReference() != 0) {
			this->runFunction(state, data->getErrorReference(), "ss", data->getError().c_str(), this->m_query.c_str());
		} else if (data->isFirstData()) {
			//This is to preserve somewhat of a backwards compatibility
			//In case people set their callbacks after they start their queries
			//If it was the first data this query has been started with then
			//it can also search on the object for the callback
			//This might break some code under very specific circumstances
			//but I doubt this will ever be an issue
			this->runCallback(state, "onError", "ss", data->getError().c_str(), this->m_query.c_str());
		}
		break;
	case QUERY_SUCCESS:
		int dataref = this->getData(state);
		if (data->getOnDataReference() != 0 || (this->hasCallback(state, "onData") && data->isFirstData())) {
			LUA->ReferencePush(dataref);
			LUA->PushNil();
			while (LUA->Next(-2)) {
				//Top is now the row, top-1 row index
				int rowReference = LUA->ReferenceCreate();
				if (data->getOnDataReference() != 0) {
					this->runFunction(state, data->getOnDataReference(), "r", rowReference);
				} else if (data->isFirstData()) {
					this->runCallback(state, "onData", "r", rowReference);
				}
				LUA->ReferenceFree(rowReference);
				//Don't have to pop since reference create consumed the value
			}
			LUA->Pop();
		}
		if (data->getSuccessReference() != 0) {
			this->runFunction(state, data->getSuccessReference(), "r", dataref);
		} else if (data->isFirstData()) {
			this->runCallback(state, "onSuccess", "r", dataref);
		}
		break;
	}
}

void Query::executeQuery(MYSQL* connection, std::shared_ptr<IQueryData> data) {
	QueryData* queryData = (QueryData*)data.get();
	this->mysqlQuery(connection, this->m_query);
	//Stores all result sets
	//MySQL result sets shouldn't be accessed from different threads!
	do {
		MYSQL_RES * results = this->mysqlStoreResults(connection);
		auto resultFree = finally([&] { mysql_free_result(results); });
		if (results != nullptr)
			queryData->m_results.emplace_back(results);
		else
			queryData->m_results.emplace_back();
		queryData->m_insertIds.push_back(mysql_insert_id(connection));
		queryData->m_affectedRows.push_back(mysql_affected_rows(connection));
	} while (this->mysqlNextResult(connection));
}

//Executes the raw query
bool Query::executeStatement(MYSQL* connection, std::shared_ptr<IQueryData> data) {
	QueryData* queryData = (QueryData*)data.get();
	queryData->setStatus(QUERY_RUNNING);
	try {
		this->executeQuery(connection, data);
		queryData->m_resultStatus = QUERY_SUCCESS;
	} catch (const MySQLException& error) {
		queryData->setError(error.what());
		queryData->setResultStatus(QUERY_ERROR);
	}
	return true;
}

//Sets the mysql query string
void Query::setQuery(std::string query) {
	m_query = query;
}


//This function just returns the data associated with the query
//Data is only created once (and then the reference to that data is returned)
int Query::getData_Wrapper(lua_State* state) {
	Query* object = (Query*)unpackSelf(state, TYPE_QUERY);
	if (!object->hasCallbackData() || object->callbackQueryData->getResultStatus() == QUERY_ERROR) {
		LUA->PushNil();
	} else {
		LUA->ReferencePush(object->getData(state));
	}
	return 1;
}

//Stores the data associated with the current result set of the query
//Only called once per result set (and then cached)
int Query::getData(lua_State* state) {
	if (this->dataReference != 0)
		return this->dataReference;
	LUA->CreateTable();
	if (hasCallbackData()) {
		QueryData* data = (QueryData*)callbackQueryData.get();
		if (data->hasMoreResults()) {
			ResultData& currentData = data->getResult();
			for (unsigned int i = 0; i < currentData.getRows().size(); i++) {
				ResultDataRow& row = currentData.getRows()[i];
				LUA->CreateTable();
				int rowObject = LUA->ReferenceCreate();
				for (unsigned int j = 0; j < row.getValues().size(); j++) {
					dataToLua(state, rowObject, j + 1, row.getValues()[j], currentData.getColumns()[j].c_str(),
						currentData.getColumnTypes()[j], row.isFieldNull(j));
				}
				LUA->PushNumber(i + 1);
				LUA->ReferencePush(rowObject);
				LUA->SetTable(-3);
				LUA->ReferenceFree(rowObject);
			}
		}
	}
	this->dataReference = LUA->ReferenceCreate();
	return this->dataReference;
};


//Function that converts the data stored in a mysql field into a lua type
void Query::dataToLua(lua_State* state, int rowReference, unsigned int column,
					  std::string &columnValue, const char* columnName, int columnType, bool isNull) {
	LUA->ReferencePush(rowReference);
	if (this->m_options & OPTION_NUMERIC_FIELDS) {
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
			LUA->PushString(columnValue.c_str(), columnValue.length());
			break;
		}
	}
	if (this->m_options & OPTION_NUMERIC_FIELDS) {
		LUA->SetTable(-3);
	} else {
		LUA->SetField(-2, columnName);
	}
	LUA->Pop();
}


//Returns true if a query has at least one additional ResultSet left
int Query::hasMoreResults(lua_State* state) {
	Query* object = (Query*)unpackSelf(state, TYPE_QUERY);
	if (!object->hasCallbackData()) {
		LUA->ThrowError("Query not completed yet");
	}
	QueryData* data = (QueryData*)object->callbackQueryData.get();
	LUA->PushBool(data->hasMoreResults());
	return 1;
}

//Unreferences the current result set and uses the next result set
int Query::getNextResults(lua_State* state) {
	Query* object = (Query*)unpackSelf(state, TYPE_QUERY);
	if (!object->hasCallbackData()) {
		LUA->ThrowError("Query not completed yet");
	}
	QueryData* data = (QueryData*)object->callbackQueryData.get();
	if (!data->getNextResults()) {
		LUA->ThrowError("Query doesn't have any more results");
	}
	if (object->dataReference != 0) {
		LUA->ReferenceFree(object->dataReference);
		object->dataReference = 0;
	}
	LUA->ReferencePush(object->getData(state));
	return 1;
}

//Returns the last insert id produced by INSERT INTO statements (or 0 if there is none)
int Query::lastInsert(lua_State* state) {
	Query* object = (Query*)unpackSelf(state, TYPE_QUERY);
	if (!object->hasCallbackData()) {
		LUA->PushNumber(0);
		return 1;
	}
	QueryData* data = (QueryData*)object->callbackQueryData.get();
	//Calling lastInsert() after query was executed but before the callback is run can cause race conditions
	LUA->PushNumber(data->getLastInsertID());
	return 1;
}

//Returns the last affected rows produced by INSERT/DELETE/UPDATE (0 for none, -1 for errors)
//For a SELECT statement this returns the amount of rows returned
int Query::affectedRows(lua_State* state) {
	Query* object = (Query*)unpackSelf(state, TYPE_QUERY);
	if (!object->hasCallbackData()) {
		LUA->PushNumber(0);
		return 1;
	}
	QueryData* data = (QueryData*)object->callbackQueryData.get();
	//Calling affectedRows() after query was executed but before the callback is run can cause race conditions
	LUA->PushNumber(data->getAffectedRows());
	return 1;
}


//When the query is destroyed by lua
void Query::onDestroyed(lua_State* state) {
	if (this->dataReference != 0 && state != nullptr) {
		//Make sure data associated with this query can be freed as well
		LUA->ReferenceFree(this->dataReference);
		this->dataReference = 0;
	}
}

std::shared_ptr<IQueryData> Query::buildQueryData(lua_State* state) {
	std::shared_ptr<IQueryData> ptr(new QueryData());
	return ptr;
}