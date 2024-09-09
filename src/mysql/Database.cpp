#include "Database.h"
#include "MySQLOOException.h"
#include <string>
#include <iostream>
#include <utility>
#include "mysqld_error.h"
#include "../lua/LuaObject.h"
#include "errmsg.h"

Database::Database(std::string host, std::string username, std::string pw, std::string database, unsigned int port,
                   std::string unixSocket) :
        database(std::move(database)), host(std::move(host)), username(std::move(username)), pw(std::move(pw)),
        socket(std::move(unixSocket)), port(port) {
    LuaObject::allocationCount++;
}

std::shared_ptr<Database>
Database::createDatabase(const std::string &host, const std::string &username, const std::string &pw,
                         const std::string &database, unsigned int port, const std::string &unixSocket) {
    return std::shared_ptr<Database>(new Database(host, username, pw, database, port, unixSocket));
}


Database::~Database() {
    this->shutdown();
    if (this->m_thread.joinable()) {
        this->m_thread.join();
    }
    LuaObject::allocationCount--;
}


//This makes sure that all stmts always get freed
std::shared_ptr<StatementHandle> Database::cacheStatement(MYSQL_STMT *stmt) {
    if (stmt == nullptr) return std::make_shared<StatementHandle>(nullptr, false);
    std::unique_lock<std::mutex> lock(m_statementMutex);
    auto handle = std::make_shared<StatementHandle>(stmt, true);
    cachedStatements.insert(handle);
    return handle;
}

//This notifies the database thread to free this statement some time in the future
void Database::freeStatement(const std::shared_ptr<StatementHandle> &handle) {
    if (handle == nullptr || !handle->isValid()) return;
    std::unique_lock<std::mutex> lock(m_statementMutex);
    if (cachedStatements.find(handle) != cachedStatements.end()) {
        //Otherwise, the statement was already freed
        cachedStatements.erase(handle);
        freedStatements.insert(handle->stmt);
    }
    handle->invalidate();
}

//Frees all statements that were allocated by the database
//This is called when the database shuts down or a reconnect happens
void Database::freeCachedStatements() {
    std::unique_lock<std::mutex> lock(m_statementMutex);
    for (auto &handle: cachedStatements) {
        if (handle == nullptr || !handle->isValid()) continue;
        mysql_stmt_close(handle->stmt);
        handle->invalidate();
    }
    cachedStatements.clear();
    for (auto &stmt: freedStatements) {
        if (stmt == nullptr) continue;
        mysql_stmt_close(stmt);
    }
    freedStatements.clear();
}

//Frees all statements that have been marked as unused, i.e. the prepared query has been destroyed.
//Called periodically by the database thread
void Database::freeUnusedStatements() {
    std::unique_lock<std::mutex> lock(m_statementMutex);
    for (auto &stmt: freedStatements) {
        //Even if this returns an error, the handle will be freed
        mysql_stmt_close(stmt);
    }
    freedStatements.clear();
}

/* Enqueues a query into the queue of accepted queries.
 */
void Database::enqueueQuery(const std::shared_ptr<IQuery> &query, const std::shared_ptr<IQueryData> &queryData) {
    queryQueue.put(std::make_pair(query, queryData));
    queryData->setStatus(QUERY_WAITING);
    this->m_queryWakeupVariable.notify_one();
}


/* Returns the amount of queued queries in the database instance
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
        if (!pair.first || !pair.second) continue;
        auto data = pair.second;
        data->setStatus(QUERY_ABORTED);
        data->setFinished(true);
    }
    return canceledQueries;
}

/* Waits for the connection of the database to finish by blocking the current thread until the connection thread finished.
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
    unsigned int nEscapedQueryLength = mysql_real_escape_string(m_sql, escapedQuery.data(), str.c_str(),
                                                                (unsigned long) str.size());
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
        return false;
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
    m_canWait = true;
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
    this->customSSLSettings = settings;
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
    shutdown();
    if (wait && m_thread.joinable()) {
        m_thread.join();
    }
}

/*
 * Returns true after the database has been fully disconnected and no more queries are in the queue.
 */
bool Database::wasDisconnected() {
    return disconnected;
}

/* Returns the status of the database, constants can be found in GMModule
 */
DatabaseStatus Database::status() const {
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
    //Ping queries do not have a lua correspondence, so they need to be removed from finished queries
    //(they are essentially just a hack)
    this->finishedQueries.removeIf(
            [queryData](std::pair<std::shared_ptr<IQuery>, std::shared_ptr<IQueryData>> const &p) {
                return p.second == queryData;
            });
    query->finishQueryData(queryData);
    return query->pingSuccess;
}

//Set this to false if your database server imposes a low prepared statements limit
//Or if you might create a very high amount of prepared queries in a short period of time
void Database::setCachePreparedStatements(bool shouldCache) {
    if (this->m_status != DATABASE_NOT_CONNECTED) {
        throw MySQLOOException("setCachePreparedStatements has to be called before db:start()!");
    }
    cachePreparedStatements = shouldCache;
}

void Database::failWaitingQuery(const std::shared_ptr<IQuery> &query, const std::shared_ptr<IQueryData> &data,
                                std::string reason) {
    data->setError(std::move(reason));
    data->setResultStatus(QUERY_ERROR);
    data->setStatus(QUERY_COMPLETE);
    data->setFinished(true);
    finishedQueries.put(std::make_pair(query, data));
}

/* Called when the database finishes running queries.
 * Aborts any waiting query. This prevents deadlocks if someone is waiting for a query to finish
 * after/while the database is shutting down
 *
 * Called from the database thread
 */
void Database::abortWaitingQuery() {
    std::unique_lock<std::mutex> lock(this->m_queryWaitMutex);
    m_canWait = false;
    auto query = this->m_waitingQuery.first;
    auto data = this->m_waitingQuery.second;
    if (query == nullptr || data == nullptr) {
        return;
    }
    failWaitingQuery(query, data, "The database of the query you were waiting on was disconnected.");
    this->m_waitingQuery = std::make_pair(nullptr, nullptr);
    this->m_queryWaitWakeupVariable.notify_all();
}

//Called from the main thread when calling query:wait()
//There can always only be at most one waiting query per database (since waiting blocks the main thread here!)
void Database::waitForQuery(const std::shared_ptr<IQuery> &query, const std::shared_ptr<IQueryData> &data) {
    {
        std::unique_lock<std::mutex> lock(this->m_queryWaitMutex);
        if (!this->m_canWait) {
            failWaitingQuery(query, data, "Can not wait on query, database is not connected or connection failed.");
            return;
        }
        if (data->isFinished()) {
            return; //No need to wait
        }
        this->m_waitingQuery = std::make_pair(query, data);
        this->m_queryWaitWakeupVariable.wait(lock, [data] { return data->isFinished(); });
    }
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
        disconnected = true;
    });
    {
        auto connectionSignaler = finally([&] { m_connectWakeupVariable.notify_one(); });
        std::lock_guard<std::mutex> lock(this->m_connectMutex);
        this->m_sql = mysql_init(nullptr);
        if (this->m_sql == nullptr) {
            m_success = false;
            m_connection_err = "Out of memory";
            m_connectionDone = true;
            m_status = DATABASE_CONNECTION_FAILED;
            return;
        }
        this->applyTimeoutSettings();
        this->customSSLSettings.applySSLSettings(this->m_sql);
        const char *socketStr = (this->socket.length() == 0) ? nullptr : this->socket.c_str();
        unsigned long clientFlag = (this->useMultiStatements) ? CLIENT_MULTI_STATEMENTS : 0;
        clientFlag |= CLIENT_MULTI_RESULTS;
        if (mysql_real_connect(this->m_sql, this->host.c_str(), this->username.c_str(), this->pw.c_str(),
                               this->database.c_str(), this->port, socketStr, clientFlag) != this->m_sql) {
            m_success = false;
            m_connection_err = mysql_error(this->m_sql);
            m_connectionDone = true;
            m_status = DATABASE_CONNECTION_FAILED;
            this->abortWaitingQuery();
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
        this->abortWaitingQuery();
    });
    if (m_success) {
        run();
    }
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
void Database::runQuery(const std::shared_ptr<IQuery>& query, const std::shared_ptr<IQueryData>& data, bool retry) {
    try {
        query->executeStatement(*this, this->m_sql, data);
        data->setResultStatus(QUERY_SUCCESS);
    } catch (const MySQLException &error) {
        unsigned int errorCode = error.getErrorCode();
        bool retryableError = errorCode == CR_SERVER_LOST || errorCode == CR_SERVER_GONE_ERROR ||
                              errorCode == ER_MAX_PREPARED_STMT_COUNT_REACHED || errorCode == ER_UNKNOWN_STMT_HANDLER ||
                              errorCode == ER_CLIENT_INTERACTION_TIMEOUT ||
                              errorCode == CR_NO_PREPARE_STMT;
        if (retry && retryableError && attemptReconnect()) {
            //Need to free statements before retrying in case the connection was lost
            //and prepared statement handles have become invalid
            freeCachedStatements();
            runQuery(query, data, false);
        } else {
            data->setResultStatus(QUERY_ERROR);
            data->setError(error.what());
        }
    }
}
#pragma clang diagnostic pop

/* The run method of the thread of the database instance.
 */
void Database::run() {
    auto a = finally([&] {
        this->freeCachedStatements();
    });
    while (true) {
        auto pair = this->queryQueue.take();
        //This detects the poison pill that is supposed to shut down the database
        if (pair.first == nullptr) {
            return;
        }
        auto curQuery = pair.first;
        auto data = pair.second;
        {
            //New scope so mutex will be released as soon as possible
            std::unique_lock<std::mutex> queryMutex(m_queryMutex);
            data->setStatus(QUERY_RUNNING);
            runQuery(curQuery, data, this->shouldAutoReconnect);
            data->setStatus(QUERY_COMPLETE);
        }
        finishedQueries.put(pair);
        {
            //Notify waiting query
            std::unique_lock<std::mutex> lock(this->m_queryWaitMutex);
            data->setFinished(true);
            auto waitingQuery = this->m_waitingQuery.first;
            auto waitingData = this->m_waitingQuery.second;
            if (waitingQuery == curQuery && waitingData == data) {
                this->m_waitingQuery = std::make_pair(nullptr, nullptr);
            }
        }
        this->m_queryWaitWakeupVariable.notify_all();
        //So that statements get eventually freed even if the queue is constantly full
        freeUnusedStatements();
    }
}

bool Database::attemptReconnect() {
    bool success;
    my_bool reconnect = '1';
    success = mariadb_reconnect(this->m_sql) == 0;
    reconnect = '0';
    mysql_optionsv(this->m_sql, MYSQL_OPT_RECONNECT, &reconnect);
    return success;
}

void Database::setConnectTimeout(unsigned int timeout) {
    this->connectTimeout = timeout;
}

void Database::setReadTimeout(unsigned int timeout) {
    this->readTimeout = timeout;
}

void Database::setWriteTimeout(unsigned int timeout) {
    this->writeTimeout = timeout;
}

void Database::applyTimeoutSettings() {
    if (this->connectTimeout > 0) {
        mysql_optionsv(this->m_sql, MYSQL_OPT_CONNECT_TIMEOUT, &this->connectTimeout);
    }
    if (this->readTimeout > 0) {
        mysql_optionsv(this->m_sql, MYSQL_OPT_READ_TIMEOUT, &this->readTimeout);
    }
    if (this->writeTimeout > 0) {
        mysql_optionsv(this->m_sql, MYSQL_OPT_WRITE_TIMEOUT, &this->writeTimeout);
    }
}

StatementHandle::StatementHandle(MYSQL_STMT *stmt, bool valid) : stmt(stmt), valid(valid) {}
