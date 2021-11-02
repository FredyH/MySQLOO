#include "IQuery.h"

#include <utility>
#include "Database.h"
#include "MySQLOOException.h"

//Important:
//Calling any query functions that rely on data from the query thread
//before the callback is called can result in race conditions.
//Always check for QUERY_COMPLETE!!!

IQuery::IQuery(std::weak_ptr<Database> database) : m_database(std::move(database)) {
    m_options = OPTION_NAMED_FIELDS | OPTION_INTERPRET_DATA | OPTION_CACHE;
}

IQuery::~IQuery() = default;

QueryResultStatus IQuery::getResultStatus() const {
    if (!hasCallbackData()) {
        return QUERY_NONE;
    }
    return callbackQueryData->getResultStatus();
}

//Wrapper for c api calls
//Just throws an exception if anything goes wrong for ease of use

void IQuery::mysqlAutocommit(MYSQL *sql, bool auto_mode) {
    my_bool result = mysql_autocommit(sql, (my_bool) auto_mode);
    if (result) {
        const char *errorMessage = mysql_error(sql);
        unsigned int errorCode = mysql_errno(sql);
        throw MySQLException(errorCode, errorMessage);
    }
}

//Returns if the query has been queued with the database instance
bool IQuery::isRunning() {
    return !runningQueryData.empty();
}

//Blocks the current Thread until the query has finished processing
//Possibly dangerous (dead lock when database goes down while waiting)
//If the second argument is set to true, the query is going to be swapped to the front of the query queue
void IQuery::wait(bool shouldSwap) {
    if (!isRunning()) {
        throw MySQLOOException("Query not started");
    }
    std::shared_ptr<IQueryData> lastInsertedQuery = runningQueryData.back();
    //Changing the order of the query might have unwanted side effects, so this is disabled by default
    auto database = m_database.lock();
    if (database && shouldSwap) {
        database->queryQueue.swapToFrontIf(
                [&](std::pair<std::shared_ptr<IQuery>, std::shared_ptr<IQueryData>> const &p) {
                    return p.second.get() == lastInsertedQuery.get();
                });
    }
    {
        std::unique_lock<std::mutex> lck(m_waitMutex);
        while (!lastInsertedQuery->isFinished()) m_waitWakeupVariable.wait(lck);
    }
}

//Returns the error message produced by the mysql query or 0 if there is none
std::string IQuery::error() const {
    if (!hasCallbackData()) {
        throw MySQLOOException("Query not started");
    }
    return callbackQueryData->getError();
}

//Attempts to abort the query, returns true if it was able to stop at least one query in time, false otherwise
std::vector<std::shared_ptr<IQueryData>> IQuery::abort() {
    auto database = m_database.lock();
    if (!database) return {};
    //This is copied so that I can remove entries from that vector in onQueryDataFinished
    auto vec = runningQueryData;
    std::vector<std::shared_ptr<IQueryData>> abortedQueries;
    for (auto &data: vec) {
        //It doesn't really matter if any of them are in a transaction since in that case they
        //aren't in the query queue
        bool wasRemoved = database->queryQueue.removeIf(
                [&](std::pair<std::shared_ptr<IQuery>, std::shared_ptr<IQueryData>> const &p) {
                    return p.second.get() == data.get();
                });
        if (wasRemoved) {
            data->setStatus(QUERY_ABORTED);
            abortedQueries.push_back(data);
        }
    }
    return abortedQueries;
}

//Sets several query options
void IQuery::setOption(int option, bool enabled) {
    if (option != OPTION_NUMERIC_FIELDS &&
        option != OPTION_NAMED_FIELDS &&
        option != OPTION_INTERPRET_DATA &&
        option != OPTION_CACHE) {
        throw MySQLOOException("Invalid Option");
    }

    if (enabled) {
        m_options |= option;
    } else {
        m_options &= ~option;
    }
}

//Wrapper for c api calls
//Just throws an exception if anything goes wrong for ease of use

void IQuery::mysqlQuery(MYSQL *sql, std::string &query) {
    int result = mysql_real_query(sql, query.c_str(), (unsigned long) query.length());
    if (result != 0) {
        const char *errorMessage = mysql_error(sql);
        unsigned int errorCode = mysql_errno(sql);
        throw MySQLException(errorCode, errorMessage);
    }
}

MYSQL_RES *IQuery::mysqlStoreResults(MYSQL *sql) {
    MYSQL_RES *result = mysql_store_result(sql);
    if (result == nullptr) {
        unsigned int errorCode = mysql_errno(sql);
        if (errorCode != 0) {
            const char *errorMessage = mysql_error(sql);
            throw MySQLException(errorCode, errorMessage);
        }
    }
    return result;
}

bool IQuery::mysqlNextResult(MYSQL *sql) {
    int result = mysql_next_result(sql);
    if (result == 0) return true;
    if (result == -1) return false;
    unsigned int errorCode = mysql_errno(sql);
    if (errorCode != 0) {
        const char *errorMessage = mysql_error(sql);
        throw MySQLException(errorCode, errorMessage);
    }
    return false;
}

//Queues the query into the queue of the database instance associated with it
void IQuery::start(const std::shared_ptr<IQueryData> &queryData) {
    if (auto db = m_database.lock()) {
        addQueryData(queryData);
        db->enqueueQuery(shared_from_this(), queryData);
        hasBeenStarted = true;
    }
}


void IQuery::addQueryData(const std::shared_ptr<IQueryData> &data) {
    if (!hasBeenStarted) {
        data->m_wasFirstData = true;
    }
    runningQueryData.push_back(data);
}

void IQuery::finishQueryData(const std::shared_ptr<IQueryData> &data) {
    runningQueryData.erase(std::remove(runningQueryData.begin(), runningQueryData.end(), data), runningQueryData.end());
}
