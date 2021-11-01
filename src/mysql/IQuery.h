#ifndef IQUERY_
#define IQUERY_

#include "MySQLHeader.h"
#include <string>
#include <mutex>
#include <atomic>
#include <utility>
#include <vector>
#include <condition_variable>

class Database;

enum QueryStatus {
    QUERY_NOT_RUNNING = 0,
    QUERY_RUNNING = 1,
    QUERY_COMPLETE = 3, //Query is complete right before callback is run
    QUERY_ABORTED = 4,
    QUERY_WAITING = 5,
};
enum QueryResultStatus {
    QUERY_NONE = 0,
    QUERY_ERROR,
    QUERY_SUCCESS
};
enum {
    OPTION_NUMERIC_FIELDS = 1,
    OPTION_NAMED_FIELDS = 2,
    OPTION_INTERPRET_DATA = 4,
    OPTION_CACHE = 8,
};

class IQueryData;

class MySQLException : public std::runtime_error {
public:
    MySQLException(unsigned int errorCode, const char *message) : runtime_error(message) {
        this->m_errorCode = errorCode;
    }

    unsigned int getErrorCode() const { return m_errorCode; }

private:
    unsigned int m_errorCode = 0;
};

class IQuery : public std::enable_shared_from_this<IQuery> {
    friend class Database;

public:
    explicit IQuery(std::weak_ptr<Database> database);

    virtual ~IQuery();

    void setCallbackData(std::shared_ptr<IQueryData> data) {
        callbackQueryData = std::move(data);
    }

    void start(const std::shared_ptr<IQueryData> &queryData);

    bool isRunning();

    void setOption(int option, bool enabled);

    bool hasOption(int option) const {
        return m_options & option;
    }

    void addQueryData(const std::shared_ptr<IQueryData> &data);

    std::string error();

    std::vector<std::shared_ptr<IQueryData>> abort();

    void wait(bool shouldSwap);

    bool hasCallbackData() const {
        return callbackQueryData != nullptr;
    }
    QueryResultStatus getResultStatus();
    std::shared_ptr<IQueryData> callbackQueryData;
protected:

    virtual bool executeStatement(Database &database, MYSQL *m_sql, std::shared_ptr<IQueryData> data) = 0;

    //Wrapper functions for c api that throw exceptions
    void mysqlQuery(MYSQL *sql, std::string &query);

    void mysqlAutocommit(MYSQL *sql, bool auto_mode);

    MYSQL_RES *mysqlStoreResults(MYSQL *sql);

    bool mysqlNextResult(MYSQL *sql);

    //fields
    std::weak_ptr<Database> m_database{};
    std::condition_variable m_waitWakeupVariable;
    std::mutex m_waitMutex;
    int m_options = 0;
    std::vector<std::shared_ptr<IQueryData>> runningQueryData;
    bool hasBeenStarted = false;
};

class IQueryData {
    friend class IQuery;

public:
    virtual ~IQueryData() = default;

    std::string getError() {
        return m_errorText;
    }

    void setError(std::string err) {
        m_errorText = std::move(err);
    }

    bool isFinished() {
        return finished;
    }

    void setFinished(bool isFinished) {
        finished = isFinished;
    }

    QueryStatus getStatus() {
        return m_status;
    }

    void setStatus(QueryStatus status) {
        this->m_status = status;
    }

    QueryResultStatus getResultStatus() {
        return m_resultStatus;
    }

    void setResultStatus(QueryResultStatus status) {
        m_resultStatus = status;
    }

    int getErrorReference() const {
        return m_errorReference;
    }

    int getSuccessReference() const {
        return m_successReference;
    }

    int getOnDataReference() const {
        return m_onDataReference;
    }

    int getAbortReference() const {
        return m_abortReference;
    }

    bool isFirstData() const {
        return m_wasFirstData;
    }
    int m_successReference = 0;
    int m_errorReference = 0;
    int m_abortReference = 0;
    int m_onDataReference = 0;
    int m_tableReference = 0;
protected:
    std::string m_errorText;
    std::atomic<bool> finished{false};
    std::atomic<QueryStatus> m_status{QUERY_NOT_RUNNING};
    std::atomic<QueryResultStatus> m_resultStatus{QUERY_NONE};
    bool m_wasFirstData = false;
};

#endif