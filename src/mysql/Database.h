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
#include <condition_variable>
#include "GarrysMod/Lua/Interface.h"
#include "../BlockingQueue.h"
#include "Query.h"
#include "PreparedQuery.h"
#include "IQuery.h"
#include "PingQuery.h"
#include "Transaction.h"

struct SSLSettings {
    bool customSSLSettings = false;
    std::string key;
    std::string cert;
    std::string ca;
    std::string capath;
    std::string cipher;

    void applySSLSettings(MYSQL *m_sql) const;
};

class DatabaseThread;

class ConnectThread;

enum DatabaseStatus {
    DATABASE_CONNECTED = 0,
    DATABASE_CONNECTING = 1,
    DATABASE_NOT_CONNECTED = 2,
    DATABASE_CONNECTION_FAILED = 3
};

class Database : public std::enable_shared_from_this<Database> {
    friend class IQuery;

public:
    enum {
        INTEGER = 0,
        BIT,
        FLOATING_POINT,
        STRING,
    };

    static std::shared_ptr<Database>
    createDatabase(const std::string &host, const std::string &username, const std::string &pw,
                   const std::string &database, unsigned int port,
                   const std::string &unixSocket);

    ~Database();

    void cacheStatement(MYSQL_STMT *stmt);

    void freeStatement(MYSQL_STMT *stmt);

    void enqueueQuery(const std::shared_ptr<IQuery> &query, const std::shared_ptr<IQueryData> &data);

    void setShouldAutoReconnect(bool autoReconnect);

    void setAutoReconnect(bool autoReconnect);

    bool getAutoReconnect();

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

    DatabaseStatus status();

    unsigned int serverVersion();

    std::string serverInfo();

    std::string hostInfo();

    size_t queueSize();

    void setMultiStatements(bool multiStatement);

    void setCachePreparedStatements(bool cachePreparedStatements);

    void disconnect(bool wait);

    void setSSLSettings(const SSLSettings &settings);
    std::atomic<DatabaseStatus> m_status{DATABASE_NOT_CONNECTED};
    std::string m_connection_err;

private:
    Database(std::string host, std::string username, std::string pw, std::string database, unsigned int port,
             std::string unixSocket);

    void shutdown();

    void freeCachedStatements();

    void freeUnusedStatements();

    void run();

    void connectRun();

    BlockingQueue<std::pair<std::shared_ptr<IQuery>, std::shared_ptr<IQueryData>>> finishedQueries{};
    BlockingQueue<std::pair<std::shared_ptr<IQuery>, std::shared_ptr<IQueryData>>> queryQueue{};
    BlockingQueue<MYSQL_STMT *> cachedStatements{};
    BlockingQueue<MYSQL_STMT *> freedStatements{};
    MYSQL *m_sql = nullptr;
    std::thread m_thread;
    std::mutex m_connectMutex; //Mutex used during connection
    std::mutex m_queryMutex; //Mutex that is locked while query thread operates on m_sql object
    std::condition_variable m_connectWakeupVariable;
    unsigned int m_serverVersion = 0;
    std::string m_serverInfo;
    std::string m_hostInfo;
    bool shouldAutoReconnect = true;
    bool useMultiStatements = true;
    bool dbCallbackRan = false;
    bool startedConnecting = false;
    bool disconnected = false;
    std::atomic<bool> m_success{true};
    std::atomic<bool> m_connectionDone{false};
    std::atomic<bool> cachePreparedStatements{true};
    std::condition_variable m_queryWakupVariable;
    std::string database;
    std::string host;
    std::string username;
    std::string pw;
    std::string socket;
    unsigned int port;
    SSLSettings customSSLSettings{};

    std::shared_ptr<PreparedQuery> prepare();
};

#endif