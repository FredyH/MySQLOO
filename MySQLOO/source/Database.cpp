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

Database::Database(GarrysMod::Lua::ILuaBase* LUA, std::string host, std::string username, std::string pw, std::string database, unsigned int port, std::string unixSocket) :
	LuaObjectBase(LUA, TYPE_DATABASE), database(database), host(host), username(username), pw(pw), socket(unixSocket), port(port) {
	classname = "Database";
	registerFunction(LUA, "prepare", Database::prepare);
	registerFunction(LUA, "escape", Database::escape);
	registerFunction(LUA, "query", Database::query);
	registerFunction(LUA, "createTransaction", Database::createTransaction);
	registerFunction(LUA, "connect", Database::connect);
	registerFunction(LUA, "abortAllQueries", Database::abortAllQueries);
	registerFunction(LUA, "wait", Database::wait);
	registerFunction(LUA, "serverVersion", Database::serverVersion);
	registerFunction(LUA, "serverInfo", Database::serverInfo);
	registerFunction(LUA, "hostInfo", Database::hostInfo);
	registerFunction(LUA, "status", Database::status);
	registerFunction(LUA, "queueSize", Database::queueSize);
	registerFunction(LUA, "setAutoReconnect", Database::setAutoReconnect);
	registerFunction(LUA, "setCachePreparedStatements", Database::setCachePreparedStatements);
	registerFunction(LUA, "setCharacterSet", Database::setCharacterSet);
	registerFunction(LUA, "setMultiStatements", Database::setMultiStatements);
	registerFunction(LUA, "ping", Database::ping);
	registerFunction(LUA, "disconnect", Database::disconnect);
}

Database::~Database() {
	this->shutdown();
	if (this->m_thread.joinable()) {
		this->m_thread.join();
	}
}


//This makes sure that all stmts always get freed
void Database::cacheStatement(MYSQL_STMT* stmt) {
	if (stmt == nullptr) return;
	cachedStatements.put(stmt);
}

//This notifies the database thread to free this statement some time in the future
void Database::freeStatement(MYSQL_STMT* stmt) {
	if (stmt == nullptr) return;
	cachedStatements.remove(stmt);
	freedStatements.put(stmt);
}

/* Creates and returns a query instance and enqueues it into the queue of accepted queries.
*/
int Database::query(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* object = (Database*)unpackSelf(LUA, TYPE_DATABASE);
	LUA->CheckType(2, GarrysMod::Lua::Type::STRING);
	unsigned int outLen = 0;
	const char* query = LUA->GetString(2, &outLen);
	Query* queryObject = new Query(object, LUA);
	queryObject->setQuery(std::string(query, outLen));
	queryObject->pushTableReference(LUA);
	return 1;
}

/* Creates and returns a PreparedQuery instance and enqueues it into the queue of accepted queries.
*/
int Database::prepare(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* object = (Database*)unpackSelf(LUA, TYPE_DATABASE);
	LUA->CheckType(2, GarrysMod::Lua::Type::STRING);
	unsigned int outLen = 0;
	const char* query = LUA->GetString(2, &outLen);
	PreparedQuery* queryObject = new PreparedQuery(object, LUA);
	queryObject->setQuery(std::string(query, outLen));
	queryObject->pushTableReference(LUA);
	return 1;
}

/* Creates and returns a PreparedQuery instance and enqueues it into the queue of accepted queries.
*/
int Database::createTransaction(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* object = (Database*)unpackSelf(LUA, TYPE_DATABASE);
	Transaction* transactionObject = new Transaction(object, LUA);
	transactionObject->pushTableReference(LUA);
	return 1;
}

/* Enqueues a query into the queue of accepted queries.
 */
void Database::enqueueQuery(IQuery* query, std::shared_ptr<IQueryData> queryData) {
	query->canbedestroyed = false;
	queryQueue.put(std::make_pair(std::dynamic_pointer_cast<IQuery>(query->getSharedPointerInstance()), queryData));
	queryData->setStatus(QUERY_WAITING);
	this->m_queryWakupVariable.notify_one();
}


/* Returns the amount of queued querys in the database instance
 * If a query is currently being processed, it does not count towards the queue size
 */
int Database::queueSize(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* object = (Database*)unpackSelf(LUA, TYPE_DATABASE);
	LUA->PushNumber(object->queryQueue.size());
	return 1;
}


/* Aborts all queries that are in the queue of started queries and returns the number of successfully aborted queries.
 * Does not abort queries that are already taken from the queue and being processed.
 */
int Database::abortAllQueries(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* object = (Database*)unpackSelf(LUA, TYPE_DATABASE);
	auto canceledQueries = object->queryQueue.clear();
	for (auto& pair : canceledQueries) {
		auto query = pair.first;
		auto data = pair.second;
		data->setStatus(QUERY_ABORTED);
		query->onQueryDataFinished(LUA, data);
	}
	LUA->PushNumber((double) canceledQueries.size());
	return 1;
}

/* Waits for the connection of the database to finish by blocking the current thread until the connect thread finished.
 * Callbacks are going to be called before this function returns
 */
int Database::wait(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* object = (Database*)unpackSelf(LUA, TYPE_DATABASE);
	if (!object->startedConnecting) {
		LUA->ThrowError("Tried to wait for database connection to finish without starting the connection!");
	}
	std::unique_lock<std::mutex> lck(object->m_connectMutex);
	while (!object->m_connectionDone) object->m_connectWakeupVariable.wait(lck);
	object->think(LUA);
	return 0;
}

/* Escapes an unescaped string using the database taking into account the characterset of the database.
 * This might break if the characterset of the database is changed after the connection was done
 */
int Database::escape(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* object = (Database*)unpackSelf(LUA, TYPE_DATABASE);
	LUA->CheckType(2, GarrysMod::Lua::Type::STRING);
	std::lock_guard<std::mutex> lock(object->m_connectMutex);
	//No query mutex needed since this doesn't use the connection at all
	if (!object->m_connectionDone || object->m_sql == nullptr) return 0;
	const char* sQuery = LUA->GetString(2);
	size_t nQueryLength = strlen(sQuery);
	//escaped string can be twice as big as original string
	//source: http://dev.mysql.com/doc/refman/5.1/en/mysql-real-escape-string.html
	std::vector<char> escapedQuery(nQueryLength * 2 + 1);
	mysql_real_escape_string(object->m_sql, escapedQuery.data(), sQuery, nQueryLength);
	LUA->PushString(escapedQuery.data());
	return 1;
}

/* Changes the character set of the connection
 * This will run the query in the server thread, which means the server will be paused
 * until the query is done.
 * This is so db:escape always has the latest value of mysql->charset
 */
int Database::setCharacterSet(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* object = (Database*)unpackSelf(LUA, TYPE_DATABASE);
	LUA->CheckType(2, GarrysMod::Lua::Type::STRING);
	if (object->m_status != DATABASE_CONNECTED) {
		LUA->ThrowError("Database needs to be connected to change charset.");
	}
	const char* charset = LUA->GetString(2);
	//This mutex makes sure we can safely use the connection to run the query
	std::unique_lock<std::mutex> lk2(object->m_queryMutex);
	if (mysql_set_character_set(object->m_sql, charset)) {
		LUA->PushBool(false);
		LUA->PushString(mysql_error(object->m_sql));
		return 1;
	} else {
		LUA->PushBool(true);
		return 1;
	}
}

/* Starts the thread that connects to the database and then handles queries.
 */
int Database::connect(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* object = (Database*)unpackSelf(LUA, TYPE_DATABASE, true);
	if (object->m_status != DATABASE_NOT_CONNECTED || object->startedConnecting) {
		LUA->ThrowError("Database already connected.");
	}
	object->startedConnecting = true;
	object->m_status = DATABASE_CONNECTING;
	object->m_thread = std::thread(&Database::connectRun, object);
	return 0;
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
int Database::disconnect(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* object = (Database*)unpackSelf(LUA, TYPE_DATABASE);
	bool wait = false;
	if (LUA->IsType(2, GarrysMod::Lua::Type::BOOL)) {
		wait = LUA->GetBool(2);
	}
	if (object->m_status != DATABASE_CONNECTED) {
		LUA->ThrowError("Database not connected.");
	}
	object->shutdown();
	if (wait && object->m_thread.joinable()) {
		object->m_thread.join();
	}
	object->disconnected = true;
	return 0;
}

/* Returns the status of the database, constants can be found in GMModule
 */
int Database::status(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* object = (Database*)unpackSelf(LUA, TYPE_DATABASE);
	LUA->PushNumber(object->m_status);
	return 1;
}

//Any of the serverInfo/Version and hostInfo functions could return outdated information if a reconnect happens
//(for example after a mysql server upgrade)

/* Returns the server version as a formatted integer (XYYZZ, X= major-, Y=minor, Z=sub-version)
 * Only works as soon as the connection has been established
 */
int Database::serverVersion(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* object = (Database*)unpackSelf(LUA, TYPE_DATABASE);
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
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* object = (Database*)unpackSelf(LUA, TYPE_DATABASE);
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
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* object = (Database*)unpackSelf(LUA, TYPE_DATABASE);
	if (!object->m_connectionDone) {
		LUA->ThrowError("Tried to get server info when client is not connected to server yet!");
	}
	LUA->PushString(object->m_hostInfo.c_str());
	return 1;
}

int Database::setAutoReconnect(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* object = (Database*)unpackSelf(LUA, TYPE_DATABASE);
	if (object->m_status != DATABASE_NOT_CONNECTED || object->startedConnecting) {
		LUA->ThrowError("Database already connected.");
	}
	LUA->CheckType(2, GarrysMod::Lua::Type::BOOL);
	object->shouldAutoReconnect = LUA->GetBool(2);
	return 0;
}

int Database::setMultiStatements(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* object = (Database*)unpackSelf(LUA, TYPE_DATABASE);
	if (object->m_status != DATABASE_NOT_CONNECTED || object->startedConnecting) {
		LUA->ThrowError("Database already connected.");
	}
	LUA->CheckType(2, GarrysMod::Lua::Type::BOOL);
	object->useMultiStatements = LUA->GetBool(2);
	return 0;
}

int Database::ping(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* database = (Database*)unpackSelf(LUA, TYPE_DATABASE);
	if (database->m_status != DATABASE_CONNECTED) {
		LUA->PushBool(false);
		return 1;
	}
	//This pretty much uses most of the lua api
	//We can't use the sql object directly since only the sql
	//thread should use it to prevent threading issues
	PingQuery* query = new PingQuery(database, LUA);
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
}

//Set this to false if your database server imposes a low prepared statements limit
//Or if you might create a very high amount of prepared queries in a short period of time
int Database::setCachePreparedStatements(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	Database* database = (Database*)unpackSelf(LUA, TYPE_DATABASE);
	if (database->m_status != DATABASE_NOT_CONNECTED) {
		LUA->ThrowError("setCachePreparedStatements has to be called before db:start()");
		return 0;
	}
	LUA->CheckType(2, GarrysMod::Lua::Type::BOOL);
	database->cachePreparedStatements = LUA->GetBool();
	return 0;
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
		if (this->shouldAutoReconnect) {
			setAutoReconnect((my_bool) 1);
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
	auto closeConnection = finally([&] {
		std::unique_lock<std::mutex> queryMutex(m_queryMutex);
		mysql_close(this->m_sql); this->m_sql = nullptr;  
	});
	if (m_success) {
		run();
	}
}

/* Think hook of the database instance.
 * In case the database connection was established or failed for the first time the connection callbacks are being run.
 * Takes all the queries from the finished queries queue and runs the callback for them.
 */
void Database::think(GarrysMod::Lua::ILuaBase* LUA) {
	if (m_connectionDone && !dbCallbackRan) {
		dbCallbackRan = true;
		if (m_success) {
			runCallback(LUA, "onConnected");
		} else {
			runCallback(LUA, "onConnectionFailed", "s", m_connection_err.c_str());
		}
		this->unreference(LUA);
	}
	//Needs to lock for condition check to prevent race conditions
	auto currentlyFinished = finishedQueries.clear();
	while (!currentlyFinished.empty()) {
		auto pair = currentlyFinished.front();
		auto query = pair.first;
		auto data = pair.second;
		currentlyFinished.pop_front();
		//Unlocking here because the lock isn't needed for the callbacks
		//Allows the database thread to add more finished queries
		query->setCallbackData(data);
		data->setStatus(QUERY_COMPLETE);
		query->doCallback(LUA, data);
		query->onQueryDataFinished(LUA, data);
	}
}

void Database::freeCachedStatements() {
	auto statments = cachedStatements.clear();
	for (auto& stmt : statments) {
		mysql_stmt_close(stmt);
	}
}

void Database::freeUnusedStatements() {
	auto statments = freedStatements.clear();
	for (auto& stmt : statments) {
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
			curquery->executeStatement(this->m_sql, data);
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