#ifndef IQUERY_
#define IQUERY_
#include "MySQLHeader.h"
#include "LuaObjectBase.h"
#include <string>
#include <mutex>
#include <atomic>
#include <vector>
#include <condition_variable>
#include "ResultData.h"
class DataRow;
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
	MySQLException(int errorCode, const char* message) : runtime_error(message) {
		this->m_errorCode = errorCode;
	}
	int getErrorCode() const { return m_errorCode; }
private:
	int m_errorCode = 0;
};

class IQuery : public LuaObjectBase {
	friend class Database;
public:
	IQuery(Database* dbase, lua_State* state);
	virtual ~IQuery();
	virtual void doCallback(lua_State* state, std::shared_ptr<IQueryData> queryData) = 0;
	virtual void onDestroyed(lua_State* state) {};
	virtual std::shared_ptr<IQueryData> buildQueryData(lua_State* state) = 0;
	void addQueryData(lua_State* state, std::shared_ptr<IQueryData> data, bool shouldRefCallbacks = true);
	void onQueryDataFinished(lua_State* state, std::shared_ptr<IQueryData> data);
	void setCallbackData(std::shared_ptr<IQueryData> data) {
		callbackQueryData = data;
	}
protected:
	//methods
	QueryResultStatus getResultStatus();
	virtual bool executeStatement(MYSQL* m_sql, std::shared_ptr<IQueryData> data) = 0;
	virtual void think(lua_State* state) {};
	static int start(lua_State* state);
	static int isRunning(lua_State* state);
	static int setOption(lua_State* state);
	static int error(lua_State* state);
	static int abort(lua_State* state);
	static int wait(lua_State* state);
	bool hasCallbackData() {
		return callbackQueryData.get() != nullptr;
	}
	//Wrapper functions for c api that throw exceptions
	void mysqlQuery(MYSQL* sql, std::string &query);
	void mysqlAutocommit(MYSQL* sql, bool auto_mode);
	MYSQL_RES* mysqlStoreResults(MYSQL* sql);
	bool mysqlNextResult(MYSQL* sql);
	//fields
	Database* m_database = nullptr;
	std::condition_variable m_waitWakeupVariable;
	int m_options = 0;
	std::vector<std::shared_ptr<IQueryData>> runningQueryData;
	std::shared_ptr<IQueryData> callbackQueryData;
	bool hasBeenStarted = false;
};

class IQueryData {
	friend class IQuery;
public:
	std::string getError() {
		return m_errorText;
	}

	void setError(std::string err) {
		m_errorText = err;
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

	int getErrorReference() {
		return m_errorReference;
	}

	int getSuccessReference() {
		return m_successReference;
	}

	int getOnDataReference() {
		return m_onDataReference;
	}

	int getAbortReference() {
		return m_abortReference;
	}

	bool isFirstData() {
		return m_wasFirstData;
	}
protected:
	std::string m_errorText = "";
	std::atomic<bool> finished{ false };
	std::atomic<QueryStatus> m_status{ QUERY_NOT_RUNNING };
	std::atomic<QueryResultStatus> m_resultStatus{ QUERY_NONE };
	int m_successReference = 0;
	int m_errorReference = 0;
	int m_abortReference = 0;
	int m_onDataReference = 0;
	bool m_wasFirstData = false;
};
#endif