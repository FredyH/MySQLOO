#ifndef DATABASE_
#define DATABASE_

#include "MySQLHeader.h"
#include <vector>
#include <deque>
#include <thread>
#include <future>
#include <mutex>
#include <sstream>
#include <condition_variable>
#include "GarrysMod/Lua/Interface.h"
#include "LuaObjectBase.h"
#include "Query.h"
#include "IQuery.h"

class DatabaseThread;
class ConnectThread;
enum DatabaseStatus
{
	DATABASE_NOT_CONNECTED = 0,
	DATABASE_CONNECTING,
	DATABASE_CONNECTED,
	DATABASE_CONNECTION_FAILED,
};

class Database : LuaObjectBase
{
	friend class IQuery;
public:
	enum
	{
		INTEGER = 0,
		BIT,
		FLOATING_POINT,
		STRING,
	};
	Database(lua_State* state, std::string host, std::string username, std::string pw, std::string database, unsigned int port, std::string unixSocket);
	~Database(void);
	void enqueueQuery(IQuery* query);
	void think(lua_State*);
private:
	void run();
	void connectRun();
	static int query(lua_State* state);
	static int prepare(lua_State* state);
	static int escape(lua_State* state);
	static int connect(lua_State* state);
	static int wait(lua_State* state);
	static int abortAllQueries(lua_State* state);
	static int status(lua_State* state);
	static int serverVersion(lua_State* state);
	static int serverInfo(lua_State* state);
	static int hostInfo(lua_State* state);
	static int queueSize(lua_State* state);
	static int setAutoReconnect(lua_State* state);
	static int setMultiStatements(lua_State* state);
	std::deque<std::shared_ptr<IQuery>> finishedQueries;
	std::deque<std::shared_ptr<IQuery>> queryQueue;
	MYSQL* m_sql;
	std::thread m_thread;
	std::mutex m_queryQueueMutex;
	std::mutex m_finishedQueueMutex;
	std::mutex m_connectMutex;
	std::condition_variable m_connectWakeupVariable;
	unsigned int m_serverVersion = 0;
	std::string m_serverInfo = "";
	std::string m_hostInfo = "";
	bool shouldAutoReconnect = true;
	bool useMultiStatements = true;
	bool dbCallbackRan = false;
	bool startedConnecting = false;
	std::atomic<bool> destroyed{ false };
	std::atomic<bool> m_success{ true };
	std::atomic<bool> m_connectionDone{ false };
	std::atomic<DatabaseStatus> m_status{ DATABASE_NOT_CONNECTED };
	std::string m_connection_err;
	std::condition_variable m_queryWakupVariable;
	std::string database = "";
	std::string host = "";
	std::string username = "";
	std::string pw = "";
	std::string socket = "";
	unsigned int port;
};
#endif