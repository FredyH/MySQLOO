#include "Transaction.h"

#include <utility>
#include "errmsg.h"
#include "Database.h"
#include "../../MySQLOO/include/LuaObjectBase.h"

bool Transaction::executeStatement(Database &database, MYSQL* connection, std::shared_ptr<IQueryData> ptr) {
    std::shared_ptr<TransactionData> data = std::dynamic_pointer_cast<TransactionData>(ptr);
	data->setStatus(QUERY_RUNNING);
	//This temporarily disables reconnect, since a reconnect
	//would rollback (and cancel) a transaction
	//Which could lead to parts of the transaction being executed outside of a transaction
	//If they are being executed after the reconnect
	bool oldReconnectStatus = database.getAutoReconnect();
    database.setAutoReconnect(false);
	auto resetReconnectStatus = finally([&] { database.setAutoReconnect(oldReconnectStatus); });
	try {
		Transaction::mysqlAutocommit(connection, false);
		{
			for (auto& query : data->m_queries) {
				try {
					//Errors are cleared in case this is retrying after losing connection
					query.second->setResultStatus(QUERY_NONE);
					query.second->setError("");
					query.first->executeQuery(database, connection, query.second);
				} catch (const MySQLException& error) {
					query.second->setError(error.what());
					query.second->setResultStatus(QUERY_ERROR);
					throw error;
				}
			}
		}
		mysql_commit(connection);
		data->setResultStatus(QUERY_SUCCESS);
		Transaction::mysqlAutocommit(connection, true);
	} catch (const MySQLException& error) {
		//This check makes sure that setting mysqlAutocommit back to true doesn't cause the transaction to fail
		//Even though the transaction was executed successfully
		if (data->getResultStatus() != QUERY_SUCCESS) {
			unsigned int errorCode = error.getErrorCode();
			if (oldReconnectStatus && !data->retried &&
				(errorCode == CR_SERVER_LOST || errorCode == CR_SERVER_GONE_ERROR)) {
				//Because autoreconnect is disabled we want to try and explicitly execute the transaction once more
				//if we can get the client to reconnect (reconnect is caused by mysql_ping)
				//If this fails we just go ahead and error
                database.setAutoReconnect(true);
				if (mysql_ping(connection) == 0) {
					data->retried = true;
					return executeStatement(database, connection, ptr);
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


std::shared_ptr<TransactionData> Transaction::buildQueryData(const std::deque<std::pair<std::shared_ptr<Query>, std::shared_ptr<IQueryData>>>& queries) {
	//At this point the transaction is guaranteed to have a referenced table
	//since this is always called shortly after transaction:start()
    return std::shared_ptr<TransactionData>(new TransactionData(queries));
}

std::shared_ptr<Transaction> Transaction::create(const std::shared_ptr<Database> &database) {
    return std::shared_ptr<Transaction>(new Transaction(database));
}
