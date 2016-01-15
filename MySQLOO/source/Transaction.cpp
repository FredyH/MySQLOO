#include "Transaction.h"
#include "ResultData.h"
#include "Logger.h"
#include "errmsg.h"

Transaction::Transaction(Database* dbase, lua_State* state) : IQuery(dbase, state) {
	registerFunction(state, "addQuery", Transaction::addQuery);
	registerFunction(state, "getQueries", Transaction::getQueries);
}

void Transaction::onDestroyed(lua_State* state) {
	//This unreferences all queries once the transaction has been gc'ed
	for (auto it = this->queries.begin(); it != this->queries.end(); it++) {
		(*it)->unreference(state);
	}
	this->queries.clear();
}

//TODO Fix memory leak if transaction is never started
int Transaction::addQuery(lua_State* state) {
	Transaction* transaction = dynamic_cast<Transaction*>(unpackSelf(state, TYPE_QUERY));
	if (transaction == NULL) {
		LUA->ThrowError("Tried to pass wrong self");
	}
	IQuery* iQuery = (IQuery*)unpackLuaObject(state, 2, TYPE_QUERY, true);
	Query* query = dynamic_cast<Query*>(iQuery);
	if (query == NULL) {
		LUA->ThrowError("Tried to pass non query to addQuery()");
	}
	std::lock_guard<std::mutex> lock(transaction->m_queryMutex);
	transaction->queries.push_back(query);
	LUA->Push(2);
	return 1;
}

int Transaction::getQueries(lua_State* state) {
	Transaction* transaction = dynamic_cast<Transaction*>(unpackSelf(state, TYPE_QUERY));
	if (transaction == NULL) {
		LUA->ThrowError("Tried to pass wrong self");
	}
	LUA->CreateTable();
	for (unsigned int i = 0; i < transaction->queries.size(); i++)
	{
		Query* q = transaction->queries[i];
		LUA->PushNumber(i + 1);
		q->pushTableReference(state);
		LUA->SetTable(-3);
	}
	return 1;
}

//Calls the lua callbacks associated with this query
void Transaction::doCallback(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	this->m_status = QUERY_COMPLETE;
	switch (this->m_resultStatus)
	{
	case QUERY_NONE:
		break;
	case QUERY_ERROR:
		this->runCallback(state, "onError", "s", this->m_errorText.c_str());
		break;
	case QUERY_SUCCESS:
		this->runCallback(state, "onSuccess");
		break;
	}
}

bool Transaction::executeStatement(MYSQL* connection)
{
	LOG_CURRENT_FUNCTIONCALL
	this->m_status = QUERY_RUNNING;
	//This temporarily disables reconnect, since a reconnect
	//would rollback (and cancel) a transaction
	//Which could lead to parts of the transaction being executed outside of a transaction
	//If they are being executed after the reconnect
	my_bool oldReconnectStatus = connection->reconnect;
	connection->reconnect = false;
	auto resetReconnectStatus = finally([&] { connection->reconnect = oldReconnectStatus; });
	try
	{
		//TODO autoreconnect fucks things up
		this->mysqlAutocommit(connection, false); 
		{
			std::lock_guard<std::mutex> lock(this->m_queryMutex);
			for (auto it = this->queries.begin(); it != this->queries.end(); it++) {
				(*it)->executeQuery(connection);
			}
		}
		mysql_commit(connection);
		this->m_resultStatus = QUERY_SUCCESS;
		this->mysqlAutocommit(connection, true);
	}
	catch (const MySQLException& error)
	{
		//This check makes sure that setting mysqlAutocommit back to true doesn't cause the transaction to fail
		//Even though the transaction was executed successfully
		if (this->m_resultStatus != QUERY_SUCCESS) {
			if (oldReconnectStatus && !this->retried && (error.getErrorCode() == CR_SERVER_LOST || error.getErrorCode() == CR_SERVER_GONE_ERROR)) {
				//Because autoreconnect is disabled we want to try and explicitly execute the transaction once more
				//if we can get the client to reconnect (reconnect is caused by mysql_ping)
				//If this fails we just go ahead and error
				connection->reconnect = true;
				if (mysql_ping(connection) == 0) {
					this->retried = true;
					return executeStatement(connection);
				}
			}
			//If this call fails it means that the connection was (probably) lost
			//In that case the mysql server rolls back any transaction anyways so it doesn't
			//matter if it fails
			mysql_rollback(connection);
			this->m_resultStatus = QUERY_ERROR;
		}
		//If this fails it probably means that the connection was lost
		//In that case autocommit is turned back on anyways (once the connection is reestablished)
		//See: https://dev.mysql.com/doc/refman/5.7/en/auto-reconnect.html
		mysql_autocommit(connection, true);
		this->m_errorText = error.what();
	}
	for (auto it = this->queries.begin(); it != this->queries.end(); it++) {
		IQuery* query = (*it);
		query->setResultStatus(this->m_resultStatus);
		query->setStatus(QUERY_COMPLETE);
	}
	this->m_status = QUERY_COMPLETE;
	return true;
}