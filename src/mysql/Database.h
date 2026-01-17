#ifndef DATABASE_
#define DATABASE_

#include "MySQLHeader.h"
#include <vector>
#include <deque>
#include <thread>
#include <future>
#include <memory>
#include <mutex>
#include <sstream>
#include "StatementHandle.h"
#include <unordered_set>
#include <condition_variable>
#include "GarrysMod/Lua/Interface.h"
#include "../BlockingQueue.h"
#include "Query.h"
#include "PreparedQuery.h"
#include "IQuery.h"
#include "PingQuery.h"
#include "Transaction.h"

struct SSLSettings {
    std::string key;
    std::string cert;
    std::string ca;
    std::string capath;
    std::string cipher;

    void applySSLSettings(MYSQL *m_sql) const;
};

enum DatabaseStatus {
    DATABASE_CONNECTED = 0,
    DATABASE_CONNECTING = 1,
    DATABASE_NOT_CONNECTED = 2,
    DATABASE_CONNECTION_FAILED = 3
};


class Database : public std::enable_shared_from_this<Database> {
    friend class IQuery;

public:
    static std::shared_ptr<Database>
    createDatabase(const std::string &host, const std::string &username, const std::string &pw,
                   const std::string &database, unsigned int port,
                   const std::string &unixSocket);

    ~Database();

    std::shared_ptr<StatementHandle> cacheStatement(MYSQL_STMT *stmt);

    void freeStatement(const std::shared_ptr<StatementHandle> &handle);

    void enqueueQuery(const std::shared_ptr<IQuery> &query, const std::shared_ptr<IQueryData> &data);

    void setShouldAutoReconnect(bool autoReconnect);

    bool shouldCachePreparedStatements() {
        return cachePreparedStatements;
    }

    std::shared_ptr<Query> query(const std::string &query);

    std::shared_ptr<PreparedQuery> prepare(const std::string &query);

    std::shared_ptr<Transaction> transaction();

    bool ping();

    std::string escape(const std::string &str);

    bool setCharacterSet(const std::string &characterSet);

    void connect();

    void wait();

    std::deque<std::pair<std::shared_ptr<IQuery>, std::shared_ptr<IQueryData>>> abortAllQueries();

    DatabaseStatus status() const;

    unsigned int serverVersion();

    std::string serverInfo();

    std::string hostInfo();

    size_t queueSize();

    void setMultiStatements(bool multiStatement);

    void setCachePreparedStatements(bool cachePreparedStatements);

    void disconnect(bool wait);

    void setConnectTimeout(unsigned int timeout);

    void setReadTimeout(unsigned int timeout);

    void setWriteTimeout(unsigned int timeout);

    void setSSLSettings(const SSLSettings &settings);

    bool isConnectionDone() { return m_connectionDone; }

    bool connectionSuccessful() { return m_success; }

    bool attemptReconnect();

    std::string connectionError() { return m_connection_err; }

    std::deque<std::pair<std::shared_ptr<IQuery>, std::shared_ptr<IQueryData>>> takeFinishedQueries() {
        return finishedQueries.clear();
    }

    bool wasDisconnected();
private:
    Database(std::string host, std::string username, std::string pw, std::string database, unsigned int port,
             std::string unixSocket);

    void shutdown();

    void freeCachedStatements();

    void freeUnusedStatements();

    void run();

    void runQuery(const std::shared_ptr<IQuery> &query, const std::shared_ptr<IQueryData> &data, bool retry);

    void connectRun();

    void abortWaitingQuery();

    void
    failWaitingQuery(const std::shared_ptr<IQuery> &query, const std::shared_ptr<IQueryData> &data, std::string reason);

    void waitForQuery(const std::shared_ptr<IQuery> &query, const std::shared_ptr<IQueryData> &data);

    void applyTimeoutSettings();

    bool attemptConnection();

    BlockingQueue<std::pair<std::shared_ptr<IQuery>, std::shared_ptr<IQueryData>>> finishedQueries{};
    BlockingQueue<std::pair<std::shared_ptr<IQuery>, std::shared_ptr<IQueryData>>> queryQueue{};
    std::unordered_set<std::shared_ptr<StatementHandle>> cachedStatements{};
    std::unordered_set<MYSQL_STMT *> freedStatements{};
    MYSQL *m_sql = nullptr;
    std::thread m_thread;
    std::mutex m_connectMutex; //Mutex used during connection
    std::mutex m_queryMutex; //Mutex that is locked while query thread operates on m_sql object
    std::mutex m_statementMutex; //Mutex that protects cached prepared statements
    std::mutex m_queryWaitMutex; //Mutex that prevents deadlocks when calling :wait()
    std::condition_variable m_connectWakeupVariable;
    unsigned int m_serverVersion = 0;
    std::string m_serverInfo;
    std::string m_hostInfo;
    std::string m_connection_err;
    bool shouldAutoReconnect = true;
    bool useMultiStatements = true;
    bool startedConnecting = false;
    bool m_canWait = false;
    std::pair<std::shared_ptr<IQuery>, std::shared_ptr<IQueryData>> m_waitingQuery = {nullptr, nullptr};
    std::atomic<bool> m_success{true};
    std::atomic<bool> disconnected { false };
    std::atomic<bool> m_connectionDone{false};
    std::atomic<bool> cachePreparedStatements{true};
    std::condition_variable m_queryWakeupVariable{};
    std::condition_variable m_queryWaitWakeupVariable{};
    std::string database;
    std::string host;
    std::string username;
    std::string pw;
    std::string socket;
    unsigned int port;
    SSLSettings customSSLSettings{};
    unsigned int readTimeout = 0;
    unsigned int writeTimeout = 0;
    unsigned int connectTimeout = 0;
    std::atomic<DatabaseStatus> m_status{DATABASE_NOT_CONNECTED};
};

#endif