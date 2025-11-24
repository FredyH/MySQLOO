#ifndef IQUERY_
#define IQUERY_

#include "MySQLHeader.h"
#include "GarrysMod/Lua/LuaBase.h"
#include <string>
#include <atomic>
#include <utility>
#include <deque>
#include <vector>
#include <condition_variable>
#include <stdexcept>

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
    explicit IQuery(std::shared_ptr<Database>  database);

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

    void finishQueryData(const std::shared_ptr<IQueryData> &data);

    std::string error() const;

    std::vector<std::shared_ptr<IQueryData>> abort();

    virtual std::string getSQLString() = 0;

    void wait(bool shouldSwap);

    bool hasCallbackData() const {
        return callbackQueryData != nullptr;
    }
    QueryResultStatus getResultStatus() const;
    std::shared_ptr<IQueryData> callbackQueryData;

    virtual void runSuccessCallback(GarrysMod::Lua::ILuaBase *LUA, const std::shared_ptr<IQueryData> &data) {};

    virtual void runErrorCallback(GarrysMod::Lua::ILuaBase *LUA, const std::shared_ptr<IQueryData> &data) {};

    virtual void runAbortedCallback(GarrysMod::Lua::ILuaBase *LUA, const std::shared_ptr<IQueryData> &data) {};
protected:

    virtual void executeStatement(Database &database, MYSQL *m_sql, const std::shared_ptr<IQueryData>& data) = 0;

    //Wrapper functions for c api that throw exceptions
    static void mysqlQuery(MYSQL *sql, std::string &query);

    static MYSQL_RES *mysqlStoreResults(MYSQL *sql);

    static bool mysqlNextResult(MYSQL *sql);

    //fields
    std::shared_ptr<Database> m_database{};
    int m_options = 0;
    std::deque<std::shared_ptr<IQueryData>> runningQueryData;
    bool hasBeenStarted = false;
};

class IQueryData : public std::enable_shared_from_this<IQueryData> {
    friend class IQuery;

public:
    virtual ~IQueryData() = default;

    std::string getError() {
        return m_errorText;
    }

    void setError(std::string err) {
        m_errorText = std::move(err);
    }

    bool isFinished();

    void setFinished(bool isFinished);

    QueryStatus getStatus();

    void setStatus(QueryStatus status);

    QueryResultStatus getResultStatus();

    void setResultStatus(QueryResultStatus status);

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

    virtual void finishLuaQueryData(GarrysMod::Lua::ILuaBase *LUA, const std::shared_ptr<IQuery> &query);
protected:
    std::string m_errorText;
    std::atomic<bool> finished{false};
    std::atomic<QueryStatus> m_status{QUERY_NOT_RUNNING};
    std::atomic<QueryResultStatus> m_resultStatus{QUERY_NONE};
    bool m_wasFirstData = false;
};

#endif