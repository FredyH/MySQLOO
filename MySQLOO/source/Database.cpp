#include "Database.h"
#include "Query.h"
#include "IQuery.h"
#include "PreparedQuery.h"
#include "PingQuery.h"
#include "Transaction.h"
#include <string>
#include <cstring>
#include <iostream>
#include <chrono>

Database::Database(lua_State* state, std::string host, std::string username, std::string pw, std::string database, unsigned int port, std::string unixSocket) :
	LuaObjectBase(state, TYPE_DATABASE), database(database), host(host), username(username), pw(pw), socket(unixSocket), port(port) {
	classname = "Database";
	registerFunction(state, "prepare", Database::prepare);
	registerFunction(state, "escape", Database::escape);
	registerFunction(state, "query", Database::query);
	registerFunction(state, "createTransaction", Database::createTransaction);
	registerFunction(state, "connect", Database::connect);
	registerFunction(state, "abortAllQueries", Database::abortAllQueries);
	registerFunction(state, "wait", Database::wait);
	registerFunction(state, "serverVersion", Database::serverVersion);
	registerFunction(state, "serverInfo", Database::serverInfo);
	registerFunction(state, "hostInfo", Database::hostInfo);
	registerFunction(state, "status", Database::status);
	registerFunction(state, "queueSize", Database::queueSize);
	registerFunction(state, "setAutoReconnect", Database::setAutoReconnect);
	registerFunction(state, "setCachePreparedStatements", Database::setCachePreparedStatements);
	registerFunction(state, "setMultiStatements", Database::setMultiStatements);
	registerFunction(state, "ping", Database::ping);
}

Database::~Database() {
	this->destroyed = true;
	{
		std::unique_lock<std::mutex> lck(m_queryQueueMutex);
		this->m_queryWakupVariable.notify_all();
	}
	if (this->m_thread.joinable()) {
		this->m_thread.join();
	}
}


//This makes sure that all stmts always get freed
void Database::cacheStatement(MYSQL_STMT* stmt) {
	if (stmt == nullptr) return;
	std::unique_lock<std::mutex> lck(m_stmtMutex);
	cachedStatements.push_back(stmt);
}

//This notifies the database thread to free this statement some time in the future
void Database::freeStatement(MYSQL_STMT* stmt) {
	if (stmt == nullptr) return;
	std::unique_lock<std::mutex> lck(m_stmtMutex);
	cachedStatements.erase(std::remove(cachedStatements.begin(), cachedStatements.end(), stmt));
	freedStatements.push_back(stmt);
}

/* Creates and returns a query instance and enqueues it into the queue of accepted queries.
*/
int Database::query(lua_State* state) {
	Database* object = (Database*)unpackSelf(state, TYPE_DATABASE);
	LUA->CheckType(2, GarrysMod::Lua::Type::STRING);
	unsigned int outLen = 0;
	const char* query = LUA->GetString(2, &outLen);
	Query* queryObject = new Query(object, state);
	queryObject->setQuery(std::string(query, outLen));
	queryObject->pushTableReference(state);
	return 1;
}

/* Creates and returns a PreparedQuery instance and enqueues it into the queue of accepted queries.
*/
int Database::prepare(lua_State* state) {
	Database* object = (Database*)unpackSelf(state, TYPE_DATABASE);
	LUA->CheckType(2, GarrysMod::Lua::Type::STRING);
	unsigned int outLen = 0;
	const char* query = LUA->GetString(2, &outLen);
	PreparedQuery* queryObject = new PreparedQuery(object, state);
	queryObject->setQuery(std::string(query, outLen));
	queryObject->pushTableReference(state);
	return 1;
}

/* Creates and returns a PreparedQuery instance and enqueues it into the queue of accepted queries.
*/
int Database::createTransaction(lua_State* state) {
	Database* object = (Database*)unpackSelf(state, TYPE_DATABASE);
	Transaction* transactionObject = new Transaction(object, state);
	transactionObject->pushTableReference(state);
	return 1;
}

/* Enqueues a query into the queue of accepted queries.
 */
void Database::enqueueQuery(IQuery* query, std::shared_ptr<IQueryData> queryData) {
	std::unique_lock<std::mutex> qlck(m_queryQueueMutex);
	query->canbedestroyed = false;
	//std::shared_ptr<IQuery> sharedPtr = query->getSharedPointerInstance();
	queryQueue.push_back(std::make_pair(std::dynamic_pointer_cast<IQuery>(query->getSharedPointerInstance()), queryData));
	queryData->setStatus(QUERY_WAITING);
	this->m_queryWakupVariable.notify_one();
}


/* Returns the amount of queued querys in the database instance
 * If a query is currently being processed, it does not count towards the queue size
 */
int Database::queueSize(lua_State* state) {
	Database* object = (Database*)unpackSelf(state, TYPE_DATABASE);
	std::unique_lock<std::mutex> qlck(object->m_queryQueueMutex);
	LUA->PushNumber(object->queryQueue.size());
	return 1;

}


/* Aborts all queries that are in the queue of started queries and returns the number of successfully aborted queries.
 * Does not abort queries that are already taken from the queue and being processed.
 */
int Database::abortAllQueries(lua_State* state) {
	Database* object = (Database*)unpackSelf(state, TYPE_DATABASE);
	std::lock_guard<std::mutex> lock(object->m_queryQueueMutex);
	for (auto& pair : object->queryQueue) {
		auto query = pair.first;
		auto data = pair.second;
		data->setStatus(QUERY_ABORTED);
		query->unreference(state);
	}
	LUA->PushNumber((double)object->queryQueue.size());
	object->queryQueue.clear();
	return 1;
}

/* Waits for the connection of the database to finish by blocking the current thread until the connect thread finished.
 * Callbacks are going to be called before this function returns
 */
int Database::wait(lua_State* state) {
	Database* object = (Database*)unpackSelf(state, TYPE_DATABASE);
	if (!object->startedConnecting) {
		LUA->ThrowError("Tried to wait for database connection to finish without starting the connection!");
	}
	std::unique_lock<std::mutex> lck(object->m_connectMutex);
	while (!object->m_connectionDone) object->m_connectWakeupVariable.wait(lck);
	object->think(state);
	return 0;
}

/* Escapes an unescaped string using the database taking into account the characterset of the database.
 * This might break if the characterset of the database is changed after the connection was done
 */
int Database::escape(lua_State* state) {
	Database* object = (Database*)unpackSelf(state, TYPE_DATABASE);
	std::lock_guard<std::mutex>(object->m_connectMutex);
	//No query mutex needed since this doesn't use the connection at all
	if (!object->m_connectionDone || object->m_sql == nullptr) return 0;
	LUA->CheckType(2, GarrysMod::Lua::Type::STRING);
	const char* sQuery = LUA->GetString(2);
	size_t nQueryLength = strlen(sQuery);
	//escaped string can be twice as big as original string
	//source: http://dev.mysql.com/doc/refman/5.1/en/mysql-real-escape-string.html
	std::vector<char> escapedQuery(nQueryLength * 2 + 1);
	mysql_real_escape_string(object->m_sql, escapedQuery.data(), sQuery, nQueryLength);
	LUA->PushString(escapedQuery.data());
	return 1;
}

/* Starts the thread that connects to the database and then handles queries.
 */
int Database::connect(lua_State* state) {
	Database* object = (Database*)unpackSelf(state, TYPE_DATABASE, true);
	if (object->m_status != DATABASE_NOT_CONNECTED || object->startedConnecting) {
		LUA->ThrowError("Database already connected.");
	}
	object->startedConnecting = true;
	object->m_status = DATABASE_CONNECTING;
	object->m_thread = std::thread(&Database::connectRun, object);
	return 0;
}

/* Returns the status of the database, constants can be found in GMModule
 */
int Database::status(lua_State* state) {
	Database* object = (Database*)unpackSelf(state, TYPE_DATABASE);
	LUA->PushNumber(object->m_status);
	return 1;
}

//Any of the serverInfo/Version and hostInfo functions could return outdated information if a reconnect happens
//(for example after a mysql server upgrade)

/* Returns the server version as a formatted integer (XYYZZ, X= major-, Y=minor, Z=sub-version)
 * Only works as soon as the connection has been established
 */
int Database::serverVersion(lua_State* state) {
	Database* object = (Database*)unpackSelf(state, TYPE_DATABASE);
	if (!object->m_connectionDone) {
		LUA->ThrowError("Tried to get server version when client is not connected to server yet!");
	}
	LUA->PushNumber(object->m_serverVersion);
	return 1;
}

/* Returns the server version as a string (for example 5.0.96)
 * Only works as soon as the connection has been established
 */
int Database::serverInfo(lua_State* state) {
	Database* object = (Database*)unpackSelf(state, TYPE_DATABASE);
	if (!object->m_connectionDone) {
		LUA->ThrowError("Tried to get server info when client is not connected to server yet!");
	}
	LUA->PushString(object->m_serverInfo.c_str());
	return 1;
}

/* Returns a string of the hostname connected to as well as the connection type
 * Only works as soon as the connection has been established
 */
int Database::hostInfo(lua_State* state) {
	Database* object = (Database*)unpackSelf(state, TYPE_DATABASE);
	if (!object->m_connectionDone) {
		LUA->ThrowError("Tried to get server info when client is not connected to server yet!");
	}
	LUA->PushString(object->m_hostInfo.c_str());
	return 1;
}

int Database::setAutoReconnect(lua_State* state) {
	Database* object = (Database*)unpackSelf(state, TYPE_DATABASE);
	if (object->m_status != DATABASE_NOT_CONNECTED || object->startedConnecting) {
		LUA->ThrowError("Database already connected.");
	}
	LUA->CheckType(2, GarrysMod::Lua::Type::BOOL);
	object->shouldAutoReconnect = LUA->GetBool(2);
	return 0;
}

int Database::setMultiStatements(lua_State* state) {
	Database* object = (Database*)unpackSelf(state, TYPE_DATABASE);
	if (object->m_status != DATABASE_NOT_CONNECTED || object->startedConnecting) {
		LUA->ThrowError("Database already connected.");
	}
	LUA->CheckType(2, GarrysMod::Lua::Type::BOOL);
	object->useMultiStatements = LUA->GetBool(2);
	return 0;
}

int Database::ping(lua_State* state) {
	Database* database = (Database*)unpackSelf(state, TYPE_DATABASE);
	if (database->m_status != DATABASE_CONNECTED) {
		LUA->PushBool(false);
		return 1;
	}
	//This pretty much uses most of the lua api
	//We can't use the sql object directly since only the sql
	//thread should use it to prevent threading issues
	PingQuery* query = new PingQuery(database, state);
	LUA->PushCFunction(IQuery::start);
	query->pushTableReference(state);
	LUA->Call(1, 0);
	//swaps the query to the front of the queryqueue to reduce wait time
	LUA->PushCFunction(IQuery::wait);
	query->pushTableReference(state);
	LUA->PushBool(true);
	LUA->Call(2, 0);
	LUA->PushBool(query->pingSuccess);
	return 1;
}

//Set this to false if your database server imposes a low prepared statements limit
//Or if you might create a very high amount of prepared queries in a short period of time
int Database::setCachePreparedStatements(lua_State* state) {
	Database* database = (Database*)unpackSelf(state, TYPE_DATABASE);
	if (database->m_status != DATABASE_NOT_CONNECTED) {
		LUA->ThrowError("setCachePreparedStatements has to be called before db:start()");
		return 0;
	}
	LUA->CheckType(2, GarrysMod::Lua::Type::BOOL);
	database->cachePreparedStatements = LUA->GetBool();
}

//Should only be called from the db thread
//While the mysql documentation says that mysql_options should only be called
//before the connection is done it appears to work after just fine (at least for reconnect)
void Database::setAutoReconnect(my_bool autoReconnect) {
	mysql_options(m_sql, MYSQL_OPT_RECONNECT, &autoReconnect);
}

//Should only be called from the db thread
my_bool Database::getAutoReconnect() {
	return m_sql->reconnect;
}


/* Thread that connects to the database, on success it continues to handle queries in the run method.
 */
void Database::connectRun() {
	mysql_thread_init();
	auto threadEnd = finally([&] { mysql_thread_end(); });
	{
		auto connectionSignaliser = finally([&] { m_connectWakeupVariable.notify_one(); });
		std::lock_guard<std::mutex>(this->m_connectMutex);
		this->m_sql = mysql_init(nullptr);
		if (this->m_sql == nullptr) {
			m_success = false;
			m_connection_err = "Out of memory";
			m_connectionDone = true;
			m_status = DATABASE_CONNECTION_FAILED;
			return;
		}
		if (this->shouldAutoReconnect) {
			setAutoReconnect((my_bool)1);
		}
		const char* socket = (this->socket.length() == 0) ? nullptr : this->socket.c_str();
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
	auto closeConnection = finally([&] { mysql_close(this->m_sql); this->m_sql = nullptr;  });
	if (m_success) {
		run();
	}
}

/* Think hook of the database instance.
 * In case the database connection was established or failed for the first time the connection callbacks are being run.
 * Takes all the queries from the finished queries queue and runs the callback for them.
 */
void Database::think(lua_State* state) {
	if (m_connectionDone && !dbCallbackRan) {
		if (m_success) {
			runCallback(state, "onConnected");
		} else {
			runCallback(state, "onConnectionFailed", "s", m_connection_err.c_str());
		}
		this->unreference(state);
		dbCallbackRan = true;
	}
	//Needs to lock for condition check to prevent race conditions
	std::unique_lock<std::mutex> lock(m_finishedQueueMutex);
	while (!finishedQueries.empty()) {
		auto pair = finishedQueries.front();
		auto query = pair.first;
		auto data = pair.second;
		finishedQueries.pop_front();
		//Unlocking here because the lock isn't needed for the callbacks
		//Allows the database thread to add more finished queries
		lock.unlock();
		query->setCallbackData(data);
		data->setStatus(QUERY_COMPLETE);
		query->doCallback(state, data);
		query->onQueryDataFinished(state, data);
		lock.lock();
	}
}

void Database::freeUnusedStatements() {
	std::lock_guard<std::mutex> stmtLock(m_stmtMutex);
	for (auto& stmt : freedStatements) {
		mysql_stmt_close(stmt);
	}
	freedStatements.clear();
}

/* The run method of the thread of the database instance.
 */
void Database::run() {
	while (true) {
		std::unique_lock<std::mutex> lock(m_queryQueueMutex);
		//Passively waiting for new queries to arrive
		while (this->queryQueue.empty() && !this->destroyed) this->m_queryWakupVariable.wait(lock);
		uint64_t counter = 0;
		//While there are new queries, execute them
		while (!this->queryQueue.empty()) {
			auto pair = this->queryQueue.front();
			auto curquery = pair.first;
			auto data = pair.second;
			this->queryQueue.pop_front();
			//The lock isn't needed for this section anymore, since it is not operating on the query queue
			lock.unlock();
			curquery->executeStatement(this->m_sql, data);
			{
				//New scope so no nested locking occurs
				std::lock_guard<std::mutex> lock(m_finishedQueueMutex);
				finishedQueries.push_back(pair);
				data->setFinished(true);
				curquery->m_waitWakeupVariable.notify_one();
			}
			//So that statements get freed sometimes even if the queue is constantly full
			if (counter++ % 200 == 0) {
				freeUnusedStatements();
			}
			lock.lock();
		}
		lock.unlock();
		freeUnusedStatements();
		lock.lock();
		if (this->destroyed && this->queryQueue.empty()) {
			std::lock_guard<std::mutex> lock(m_stmtMutex);
			for (auto& stmt : cachedStatements) {
				mysql_stmt_close(stmt);
			}
			cachedStatements.clear();
			for (auto& stmt : freedStatements) {
				mysql_stmt_close(stmt);
			}
			freedStatements.clear();
			return;
		}
	}
}