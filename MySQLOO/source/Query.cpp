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

//Executes the raw query
bool Query::executeStatement(MYSQL* connection)
{
	LOG_CURRENT_FUNCTIONCALL
	this->m_status = QUERY_RUNNING;
	try
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
			this->insertIds.push_back(mysql_insert_id(connection));
		}while (this->mysqlNextResult(connection));
		this->m_resultStatus = QUERY_SUCCESS;
	}
	catch (const MySQLException& error)
	{
		this->m_resultStatus = QUERY_ERROR;
		this->m_errorText = error.what();
	}
	return true;
}