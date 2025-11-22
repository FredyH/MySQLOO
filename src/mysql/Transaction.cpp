#include "Transaction.h"

#include <utility>
#include "Database.h"
#include "mysqld_error.h"


void Transaction::executeStatement(Database &database, MYSQL *connection, const std::shared_ptr<IQueryData>& ptr) {
    std::shared_ptr<TransactionData> data = std::dynamic_pointer_cast<TransactionData>(ptr);
    data->setStatus(QUERY_RUNNING);
    try {
        for (auto &query: data->m_queries) {
            //Errors and data are cleared in case this is retrying after losing connection
            query.second->setStatus(QUERY_RUNNING);
            query.second->setResultStatus(QUERY_NONE);
            query.second->setError("");
            Query::clearResultData(query.second);
        }

        mysqlAutocommit(connection, false);

        for (auto &query: data->m_queries) {
            try {
                query.first->executeStatement(database, connection, query.second);
                query.second->setStatus(QUERY_COMPLETE);
                query.second->setResultStatus(QUERY_SUCCESS);
            } catch (const MySQLException &error) {
                query.second->setError(error.what());
                query.second->setResultStatus(QUERY_ERROR);
                throw error;
            }
        }

        mysqlCommit(connection);
        data->setResultStatus(QUERY_SUCCESS);
        //If this fails the connection was lost but the transaction was already executed fully
        //We do not want to throw an error here so the result is ignored.
        mysql_autocommit(connection, true);

        for (const auto &pair: data->m_queries) {
            pair.second->setResultStatus(data->getResultStatus());
            pair.second->setStatus(QUERY_COMPLETE);
        }
    } catch (const MySQLException &error) {
        data->setResultStatus(QUERY_ERROR);
        mysql_rollback(connection);

        //If this fails it probably means that the connection was lost
        //In that case autocommit is turned back on anyway (once the connection is reestablished)
        mysql_autocommit(connection, true);

        //In case of reconnect this might get called twice, but this should not affect anything
        for (auto &query: data->m_queries) {
            // If an error occurs, then the queries in the transactions after the query that caused the error will
            // not have their data set yet.
            // In that case, we will make sure they have the correct data set here.
            query.second->setResultStatus(QUERY_ERROR);

            if (query.second->getStatus() == QUERY_RUNNING) {
                // The query did not have a result set yet because it never finished, so we need to give it an empty one
                Query::emplaceEmptyResultData(query.second);
                query.second->setStatus(QUERY_ABORTED);
            } else {
                query.second->setStatus(QUERY_COMPLETE);
            }
        }
        throw error;
    }
}

void Transaction::mysqlAutocommit(MYSQL *sql, bool auto_mode) {
    my_bool result = mysql_autocommit(sql, (my_bool) auto_mode);
    if (result) {
        const char *errorMessage = mysql_error(sql);
        unsigned int errorCode = mysql_errno(sql);
        throw MySQLException(errorCode, errorMessage);
    }
}

void Transaction::mysqlCommit(MYSQL *sql) {
    if (mysql_commit(sql)) {
        const char *errorMessage = mysql_error(sql);
        unsigned int errorCode = mysql_errno(sql);
        throw MySQLException(errorCode, errorMessage);
    }
}


std::shared_ptr<TransactionData>
Transaction::buildQueryData(const std::deque<std::pair<std::shared_ptr<Query>, std::shared_ptr<IQueryData>>> &queries) {
    //At this point the transaction is guaranteed to have a referenced table
    //since this is always called shortly after transaction:start()
    return std::shared_ptr<TransactionData>(new TransactionData(queries));
}

std::shared_ptr<Transaction> Transaction::create(const std::shared_ptr<Database> &database) {
    return std::shared_ptr<Transaction>(new Transaction(database));
}

void TransactionData::finishLuaQueryData(GarrysMod::Lua::ILuaBase *LUA, const std::shared_ptr <IQuery> &query) {
    IQueryData::finishLuaQueryData(LUA, query);
    for (const auto& entry : m_queries) {
        entry.second->finishLuaQueryData(LUA, entry.first);
    }
}
