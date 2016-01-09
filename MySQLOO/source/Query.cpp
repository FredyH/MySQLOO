#include "Query.h"
#include "Logger.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#ifdef LINUX
#include <stdlib.h>
#endif

Query::Query(Database* dbase, lua_State* state) : IQuery(dbase, state)
{
	classname = "Query";
}

Query::~Query(void)
{
}

//Calls the lua callbacks associated with this query
void Query::doCallback(lua_State* state)
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

void Query::executeQuery(MYSQL* connection)
{
	this->mysqlQuery(connection, this->m_query);
	//Stores all result sets
	//MySQL result sets shouldn't be accessed from different threads!
	do
	{
		MYSQL_RES * results = this->mysqlStoreResults(connection);
		auto resultFree = finally([&] { mysql_free_result(results); });
		if (results != NULL)
			this->results.emplace_back(results);
		else
			this->results.emplace_back();
		this->m_insertIds.push_back(mysql_insert_id(connection));
		this->m_affectedRows.push_back(mysql_affected_rows(connection));
	} while (this->mysqlNextResult(connection));
}

//Executes the raw query
bool Query::executeStatement(MYSQL* connection)
{
	LOG_CURRENT_FUNCTIONCALL
	this->m_status = QUERY_RUNNING;
	try
	{
		this->executeQuery(connection);
		this->m_resultStatus = QUERY_SUCCESS;
	}
	catch (const MySQLException& error)
	{
		this->m_resultStatus = QUERY_ERROR;
		this->m_errorText = error.what();
	}
	return true;
}

//Sets the mysql query string
void Query::setQuery(std::string query)
{
	m_query = query;
}
