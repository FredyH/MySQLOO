#include "Database.h"
#include "Query.h"
#include "IQuery.h"
#include "MySQLOOException.h"
#include "Transaction.h"
#include <string>
#include <cstring>
#include <iostream>
#include <chrono>
#include <utility>

Database::Database(std::string host, std::string username, std::string pw, std::string database, unsigned int port,
                   std::string unixSocket) :
        database(std::move(database)), host(std::move(host)), username(std::move(username)), pw(std::move(pw)),
        socket(std::move(unixSocket)), port(port) {

}

std::shared_ptr<Database> Database::createDatabase(const std::string &host, const std::string &username, const std::string &pw,
                         const std::string &database, unsigned int port, const std::string &unixSocket) {
    return std::shared_ptr<Database>(new Database(host, username, pw, database, port, unixSocket));
}


Database::~Database() {
    this->shutdown();
    if (this->m_thread.joinable()) {
        this->m_thread.join();
    }
}


//This makes sure that all stmts always get freed
void Database::cacheStatement(MYSQL_STMT *stmt) {
    if (stmt == nullptr) return;
    cachedStatements.put(stmt);
}

//This notifies the database thread to free this statement some time in the future
void Database::freeStatement(MYSQL_STMT *stmt) {
    if (stmt == nullptr) return;
    cachedStatements.remove(stmt);
    freedStatements.put(stmt);
}

/* Creates and returns a query instance and enqueues it into the queue of accepted queries.
*/
/*
int Database::query(lua_State *state) {
    GarrysMod::Lua::ILuaBase *LUA = state->luabase;
    LUA->SetState(state);
    Database *object = (Database *) unpackSelf(LUA, TYPE_DATABASE);
    LUA->CheckType(2, GarrysMod::Lua::Type::STRING);
    unsigned int outLen = 0;
    const char *query = LUA->GetString(2, &outLen);
    Query *queryObject = new Query(object, LUA);
    queryObject->setQuery(std::string(query, outLen));
    queryObject->pushTableReference(LUA);
    return 1;
}*/

/* Creates and returns a PreparedQuery instance and enqueues it into the queue of accepted queries.
*/
/**/


/* Creates and returns a PreparedQuery instance and enqueues it into the queue of accepted queries.
*/
/*
int Database::createTransaction(lua_State *state) {
    GarrysMod::Lua::ILuaBase *LUA = state->luabase;
    LUA->SetState(state);
    Database *object = (Database *) unpackSelf(LUA, TYPE_DATABASE);
    Transaction *transactionObject = new Transaction(object, LUA);
    transactionObject->pushTableReference(LUA);
    return 1;
}*/

/* Enqueues a query into the queue of accepted queries.
 */
void Database::enqueueQuery(const std::shared_ptr<IQuery> &query, const std::shared_ptr<IQueryData> &queryData) {
    queryQueue.put(std::make_pair(query, queryData));
    queryData->setStatus(QUERY_WAITING);
    this->m_queryWakupVariable.notify_one();
}


/* Returns the amount of queued querys in the database instance
 * If a query is currently being processed, it does not count towards the queue size
 */
size_t Database::queueSize() {
    return queryQueue.size();
}


/* Aborts all queries that are in the queue of started queries and returns the number of successfully aborted queries.
 * Does not abort queries that are already taken from the queue and being processed.
 */
std::deque<std::pair<std::shared_ptr<IQuery>, std::shared_ptr<IQueryData>>> Database::abortAllQueries() {
    auto canceledQueries = queryQueue.clear();
    for (auto &pair: canceledQueries) {
        auto query = pair.first;
        auto data = pair.second;
        data->setStatus(QUERY_ABORTED);
    }
    return canceledQueries;
}

/* Waits for the connection of the database to finish by blocking the current thread until the connect thread finished.
 */
void Database::wait() {
    if (!startedConnecting) {
        throw MySQLOOException(
                "Tried to wait for database connection to finish without starting the connection!");
    }
    std::unique_lock<std::mutex> lck(m_connectMutex);
    while (!m_connectionDone) m_connectWakeupVariable.wait(lck);
}

/* Escapes an unescaped string using the database taking into account the charset of the database.
 * This might break if the charset of the database is changed after the connection was done
 */
std::string Database::escape(const std::string &str) {
    //No query mutex needed since this doesn't use the connection at all
    std::lock_guard<std::mutex> lock(m_connectMutex);
    if (!m_connectionDone || m_sql == nullptr) {
        throw MySQLOOException("Cannot escape using database that is not connected");
    }

    //escaped string can be twice as big as original string
    //source: http://dev.mysql.com/doc/refman/5.1/en/mysql-real-escape-string.html
    std::vector<char> escapedQuery(str.size() * 2 + 1);
    unsigned int nEscapedQueryLength = mysql_real_escape_string(m_sql, escapedQuery.data(), str.c_str(), str.size());
    return {escapedQuery.data(), (size_t) nEscapedQueryLength};
}

/* Changes the character set of the connection
 * This will run the query in the server thread, which means the server will be paused
 * until the query is done.
 * This is so db:escape always has the latest value of mysql->charset
 */
bool Database::setCharacterSet(const std::string &characterSet) {
    if (this->m_status != DATABASE_CONNECTED) {
        throw MySQLOOException("Database needs to be connected to change charset.");
    }
    //This mutex makes sure we can safely use the connection to run the query
    std::unique_lock<std::mutex> lk2(m_queryMutex);
    if (mysql_set_character_set(m_sql, characterSet.c_str())) {
        return false; //TODO: Also return error?
    } else {
        return true;
    }
}

/* Starts the thread that connects to the database and then handles queries.
 */
void Database::connect() {
    if (m_status != DATABASE_NOT_CONNECTED || startedConnecting) {
        throw MySQLOOException("Database already connected.");
    }
    startedConnecting = true;
    m_status = DATABASE_CONNECTING;
    m_thread = std::thread(&Database::connectRun, this);
}

void SSLSettings::applySSLSettings(MYSQL *m_sql) const {
    if (!this->key.empty()) {
        mysql_options(m_sql, MYSQL_OPT_SSL_KEY, this->key.c_str());
    }
    if (!this->cert.empty()) {
        mysql_options(m_sql, MYSQL_OPT_SSL_CERT, this->cert.c_str());
    }
    if (!this->ca.empty()) {
        mysql_options(m_sql, MYSQL_OPT_SSL_CA, this->ca.c_str());
    }
    if (!this->capath.empty()) {
        mysql_options(m_sql, MYSQL_OPT_SSL_CAPATH, this->capath.c_str());
    }
    if (!this->cipher.empty()) {
        mysql_options(m_sql, MYSQL_OPT_SSL_CIPHER, this->cipher.c_str());
    }
}

/* Manually sets the instances SSL settings to use a custom SSL key, certificate, CA, CAPath, cipher, etc.
 * This has to be called before connecting to the database.
 */
void Database::setSSLSettings(const SSLSettings &settings) {
    if (m_status != DATABASE_NOT_CONNECTED) {
        throw MySQLOOException("Cannot set SSL settings after connecting!");
    }
    settings.applySSLSettings(this->m_sql);
}


void Database::shutdown() {
    //This acts as a poison pill
    this->queryQueue.put(std::make_pair(std::shared_ptr<IQuery>(), std::shared_ptr<IQueryData>()));
    //The fact that C++ can't automatically infer the types of the shared_ptr here and that
    //I have to specify what type it should be, just proves once again that C++ is a failed language
    //that should be replaced as soon as possible
}

/* Disconnects from the mysql database after finishing all queued queries
 * If wait is true, this will wait for the all queries to finish execution and the
 * database thread to end.
 */
void Database::disconnect(bool wait) {
    if (m_status != DATABASE_CONNECTED) {
        throw MySQLOOException("Database not connected.");
    }
    shutdown();
    if (wait && m_thread.joinable()) {
        m_thread.join();
    }
    disconnected = true;
}

/* Returns the status of the database, constants can be found in GMModule
 */
DatabaseStatus Database::status() {
    return m_status;
}

//Any of the serverInfo/Version and hostInfo functions could return outdated information if a reconnect happens
//(for example after a mysql server upgrade)

/* Returns the server version as a formatted integer (XYYZZ, X= major-, Y=minor, Z=sub-version)
 * Only works as soon as the connection has been established
 */
unsigned int Database::serverVersion() {
    if (!m_connectionDone) {
        throw MySQLOOException("Tried to get server version when client is not connected to server yet!");
    }
    return m_serverVersion;
}

/* Returns the server version as a string (for example 5.0.96)
 * Only works as soon as the connection has been established
 */
std::string Database::serverInfo() {
    if (!m_connectionDone) {
        throw MySQLOOException("Tried to get server info when client is not connected to server yet!");
    }
    return m_serverInfo;
}

/* Returns a string of the hostname connected to as well as the connection type
 * Only works as soon as the connection has been established
 */
std::string Database::hostInfo() {
    if (!m_connectionDone) {
        throw MySQLOOException("Tried to get server info when client is not connected to server yet!");
    }
    return m_hostInfo;
}

void Database::setShouldAutoReconnect(bool autoReconnect) {
    if (m_status != DATABASE_NOT_CONNECTED || startedConnecting) {
        throw MySQLOOException("Database already connected.");
    }
    shouldAutoReconnect = autoReconnect;
}

void Database::setMultiStatements(bool multiStatement) {
    if (m_status != DATABASE_NOT_CONNECTED || startedConnecting) {
        throw MySQLOOException("Database already connected.");
    }
    useMultiStatements = multiStatement;
}

std::shared_ptr<Query> Database::query(const std::string &query) {
    return std::shared_ptr<Query>(new Query(shared_from_this(), query));
}

std::shared_ptr<PreparedQuery> Database::prepare(const std::string &query) {
    return std::shared_ptr<PreparedQuery>(new PreparedQuery(shared_from_this(), query));
}

std::shared_ptr<Transaction> Database::transaction() {
    return std::shared_ptr<Transaction>(new Transaction(shared_from_this()));
}

bool Database::ping() {
    auto query = std::shared_ptr<PingQuery>(new PingQuery(shared_from_this()));
    auto queryData = query->buildQueryData();
    query->start(queryData);
    query->wait(true);
    return query->pingSuccess;
}

/*
int Database::ping(lua_State *state) {
    GarrysMod::Lua::ILuaBase *LUA = state->luabase;
    LUA->SetState(state);
    Database *database = (Database *) unpackSelf(LUA, TYPE_DATABASE);
    if (database->m_status != DATABASE_CONNECTED) {
        LUA->PushBool(false);
        return 1;
    }
    //This pretty much uses most of the lua api
    //We can't use the sql object directly since only the sql
    //thread should use it to prevent threading issues
    PingQuery *query = new PingQuery(database, LUA);
    LUA->PushCFunction(IQuery::start);
    query->pushTableReference(LUA);
    LUA->Call(1, 0);
    //swaps the query to the front of the queryqueue to reduce wait time
    LUA->PushCFunction(IQuery::wait);
    query->pushTableReference(LUA);
    LUA->PushBool(true);
    LUA->Call(2, 0);
    LUA->PushBool(query->pingSuccess);
    return 1;
}*/

//Set this to false if your database server imposes a low prepared statements limit
//Or if you might create a very high amount of prepared queries in a short period of time
void Database::setCachePreparedStatements(bool shouldCache) {
    if (this->m_status != DATABASE_NOT_CONNECTED) {
        throw MySQLOOException("setCachePreparedStatements has to be called before db:start()!");
    }
    cachePreparedStatements = shouldCache;
}

//Should only be called from the db thread
//While the mysql documentation says that mysql_options should only be called
//before the connection is done it appears to work after just fine (at least for reconnect)
void Database::setAutoReconnect(bool shouldReconnect) {
    auto myAutoReconnectBool = (my_bool) shouldReconnect;
    mysql_optionsv(m_sql, MYSQL_OPT_RECONNECT, &myAutoReconnectBool);
}

//Should only be called from the db thread
bool Database::getAutoReconnect() {
    my_bool autoReconnect;
    mysql_get_optionv(m_sql, MYSQL_OPT_RECONNECT, &autoReconnect);
    return (bool) autoReconnect;
}


/* Thread that connects to the database, on success it continues to handle queries in the run method.
 */
void Database::connectRun() {
    mysql_thread_init();
    auto threadEnd = finally([&] {
        mysql_thread_end();
        if (m_status == DATABASE_CONNECTED) {
            m_status = DATABASE_NOT_CONNECTED;
        }
    });
    {
        auto connectionSignaliser = finally([&] { m_connectWakeupVariable.notify_one(); });
        std::lock_guard<std::mutex> lock(this->m_connectMutex);
        this->m_sql = mysql_init(nullptr);
        if (this->m_sql == nullptr) {
            m_success = false;
            m_connection_err = "Out of memory";
            m_connectionDone = true;
            m_status = DATABASE_CONNECTION_FAILED;
            return;
        }
        this->customSSLSettings.applySSLSettings(this->m_sql);
        if (this->shouldAutoReconnect) {
            setAutoReconnect(true);
        }
        const char *socket = (this->socket.length() == 0) ? nullptr : this->socket.c_str();
        unsigned long clientFlag = (this->useMultiStatements) ? CLIENT_MULTI_STATEMENTS : 0;
        clientFlag |= CLIENT_MULTI_RESULTS;
        if (mysql_real_connect(this->m_sql, this->host.c_str(), this->username.c_str(), this->pw.c_str(),
                               this->database.c_str(), this->port, socket, clientFlag) != this->m_sql) {
            m_success = false;
            m_connection_err = mysql_error(this->m_sql);
            m_connectionDone = true;
            m_status = DATABASE_CONNECTION_FAILED;
            return;
        }
        m_success = true;
        m_connection_err = "";
        m_connectionDone = true;
        m_status = DATABASE_CONNECTED;
        m_serverVersion = mysql_get_server_version(this->m_sql);
        m_serverInfo = mysql_get_server_info(this->m_sql);
        m_hostInfo = mysql_get_host_info(this->m_sql);
    }
    auto closeConnection = finally([&] {
        std::unique_lock<std::mutex> queryMutex(m_queryMutex);
        mysql_close(this->m_sql);
        this->m_sql = nullptr;
    });
    if (m_success) {
        run();
    }
}

void Database::freeCachedStatements() {
    auto statments = cachedStatements.clear();
    for (auto &stmt: statments) {
        mysql_stmt_close(stmt);
    }
}

void Database::freeUnusedStatements() {
    auto statments = freedStatements.clear();
    for (auto &stmt: statments) {
        mysql_stmt_close(stmt);
    }
}

/* The run method of the thread of the database instance.
 */
void Database::run() {
    auto a = finally([&] {
        this->freeUnusedStatements();
        this->freeCachedStatements();
    });
    while (true) {
        auto pair = this->queryQueue.take();
        //This detects the poison pill that is supposed to shutdown the database
        if (pair.first.get() == nullptr) {
            return;
        }
        auto curquery = pair.first;
        auto data = pair.second;
        {
            //New scope so mutex will be released as soon as possible
            std::unique_lock<std::mutex> queryMutex(m_queryMutex);
            curquery->executeStatement(*this, this->m_sql, data);
        }
        data->setFinished(true);
        finishedQueries.put(pair);
        {
            std::unique_lock<std::mutex> queryMutex(curquery->m_waitMutex);
            curquery->m_waitWakeupVariable.notify_one();
        }
        //So that statements get freed sometimes even if the queue is constantly full
        freeUnusedStatements();
    }
}