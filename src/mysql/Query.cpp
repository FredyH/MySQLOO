#include "Query.h"
#include "MySQLOOException.h"
#include <iostream>
#include <algorithm>
#include <utility>
#ifdef LINUX
#include <stdlib.h>
#endif

Query::Query(const std::shared_ptr<Database>& dbase, std::string query) : IQuery(dbase), m_query(std::move(query)) {
}

Query::~Query() = default;

void Query::executeQuery(Database &database, MYSQL* connection, const std::shared_ptr<IQueryData> &data) {
    auto queryData = std::dynamic_pointer_cast<QueryData>(data);
	Query::mysqlQuery(connection, this->m_query);
	//Stores all result sets
	//MySQL result sets shouldn't be accessed from different threads!
	do {
		MYSQL_RES * results = Query::mysqlStoreResults(connection);
		auto resultFree = finally([&] { mysql_free_result(results); });
		if (results != nullptr) {
			queryData->m_results.emplace_back(results);
		} else {
			queryData->m_results.emplace_back();
		}
		queryData->m_insertIds.push_back(mysql_insert_id(connection));
		queryData->m_affectedRows.push_back(mysql_affected_rows(connection));
	} while (Query::mysqlNextResult(connection));
}

//Executes the raw query
bool Query::executeStatement(Database &database, MYSQL* connection, std::shared_ptr<IQueryData> data) {
    auto queryData = std::dynamic_pointer_cast<QueryData>(data);
    data->setStatus(QUERY_RUNNING);
	try {
		this->executeQuery(database, connection, data);
        queryData->m_resultStatus = QUERY_SUCCESS;
	} catch (const MySQLException& error) {
        data->setError(error.what());
        data->setResultStatus(QUERY_ERROR);
	}
	return true;
}


//Returns true if a query has at least one additional ResultSet left
bool Query::hasMoreResults() {
	if (!hasCallbackData()) {
        throw MySQLOOException("Query not completed yet");
	}
    auto data = std::dynamic_pointer_cast<QueryData>(this->callbackQueryData);
    return data->hasMoreResults();
}

//Unreferences the current result set and uses the next result set
void Query::getNextResults() {
	if (!hasCallbackData()) {
        throw MySQLOOException("Query not completed yet");
	}
    auto data = std::dynamic_pointer_cast<QueryData>(this->callbackQueryData);
	if (!data->getNextResults()) {
        throw MySQLOOException("Query doesn't have any more results");
	}
}

//Returns the last insert id produced by INSERT INTO statements (or 0 if there is none)
my_ulonglong Query::lastInsert() {
	if (!hasCallbackData()) {
        return 0;
	}
	auto data = std::dynamic_pointer_cast<QueryData>(this->callbackQueryData);
	//Calling lastInsert() after query was executed but before the callback is run can cause race conditions
    return data->getLastInsertID();
}

//Returns the last affected rows produced by INSERT/DELETE/UPDATE (0 for none, -1 for errors)
//For a SELECT statement this returns the amount of rows returned
my_ulonglong Query::affectedRows() {
    if (!hasCallbackData()) {
        return 0;
    }
    auto data = std::dynamic_pointer_cast<QueryData>(this->callbackQueryData);
	//Calling affectedRows() after query was executed but before the callback is run can cause race conditions
    return data->getAffectedRows();
}

std::shared_ptr<QueryData> Query::buildQueryData() {
    return std::shared_ptr<QueryData>(new QueryData());
}

std::shared_ptr<Query> Query::create(const std::shared_ptr<Database> &dbase, const std::string& query) {
    return std::shared_ptr<Query>(new Query(dbase, query));
}

