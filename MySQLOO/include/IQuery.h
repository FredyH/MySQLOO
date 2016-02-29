#ifndef IQUERY_
#define IQUERY_
#include "MySQLHeader.h"
#include "LuaObjectBase.h"
#include <string>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "ResultData.h"
class DataRow;
class Database;

struct QueryResultData
{
	std::string error = "";
	bool finished = false;
	bool failed = false;
	unsigned int errno;
	unsigned int affectedRows;
};

enum QueryStatus
{
	QUERY_NOT_RUNNING = 0,
	QUERY_WAITING = 1,
	QUERY_RUNNING = 2,
	QUERY_ABORTED = 3,
	QUERY_COMPLETE = 4 //Query is complete right before callback is run
};
enum QueryResultStatus
{
	QUERY_NONE = 0,
	QUERY_ERROR,
	QUERY_SUCCESS
};
enum
{
	OPTION_NUMERIC_FIELDS = 1,
	OPTION_NAMED_FIELDS = 2,
	OPTION_INTERPRET_DATA = 4,
	OPTION_CACHE = 8,
};

class MySQLException : public std::runtime_error
{
public:
	MySQLException(int errorCode, const char* message) : runtime_error(message)
	{
		this->m_errorCode = errorCode;
	}
	int getErrorCode() const { return m_errorCode; }
private:
	int m_errorCode = 0;
};

class IQuery : public LuaObjectBase
{
friend class Database;
public:
	IQuery(Database* dbase, lua_State* state);
	virtual ~IQuery();
	virtual void doCallback(lua_State* state) = 0;
	void onDestroyed(lua_State* state);
	void setResultStatus(QueryResultStatus);
	void setStatus(QueryStatus);
protected:
	//methods
	QueryResultStatus m_resultStatus = QUERY_NONE;
	QueryResultStatus getResultStatus();
	virtual bool executeStatement(MYSQL* m_sql) = 0;
	void dataToLua(lua_State* state, int rowReference, unsigned int column, std::string &columnValue, const char* columnName, int columnType, bool isNull);
	virtual void think(lua_State* state) {};
	static int start(lua_State* state);
	static int isRunning(lua_State* state);
	static int lastInsert(lua_State* state);
	static int affectedRows(lua_State* state);
	static int getData_Wrapper(lua_State* state);
	static int hasMoreResults(lua_State* state);
	static int getNextResults(lua_State* state);
	static int setOption(lua_State* state);
	static int wait(lua_State* state);
	static int error(lua_State* state);
	static int abort(lua_State* state);
	int getData(lua_State* state);
	//Wrapper functions for c api that throw exceptions
	void mysqlQuery(MYSQL* sql, std::string &query);
	void mysqlAutocommit(MYSQL* sql, bool auto_mode);
	MYSQL_RES* mysqlStoreResults(MYSQL* sql);
	bool mysqlNextResult(MYSQL* sql);
	//fields
	Database* m_database = nullptr;
	std::atomic<bool> finished{ false };
	std::atomic<QueryStatus> m_status{ QUERY_NOT_RUNNING };
	std::string m_errorText = "";
	std::deque<my_ulonglong> m_affectedRows;
	std::deque<my_ulonglong> m_insertIds;
	std::deque<ResultData> results;
	std::condition_variable m_waitWakeupVariable;
	int m_options = 0;
	int dataReference = 0;
};
#endif