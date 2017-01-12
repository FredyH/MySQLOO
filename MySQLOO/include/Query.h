#ifndef QUERY_
#define QUERY_
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "IQuery.h"

class QueryData;
class Query : public IQuery {
	friend class Database;
public:
	Query(Database* dbase, lua_State* state);
	virtual ~Query(void);
	void setQuery(std::string query);
	virtual bool executeStatement(MYSQL* m_sql, std::shared_ptr<IQueryData> data);
	virtual void executeQuery(MYSQL* m_sql, std::shared_ptr<IQueryData> data);
	virtual void onDestroyed(lua_State* state);
	virtual void doCallback(lua_State* state, std::shared_ptr<IQueryData> queryData);
	virtual std::shared_ptr<IQueryData> buildQueryData(lua_State* state);
protected:
	void dataToLua(lua_State* state, int rowReference, unsigned int column, std::string &columnValue, const char* columnName, int columnType, bool isNull);
	static int lastInsert(lua_State* state);
	static int affectedRows(lua_State* state);
	static int getData_Wrapper(lua_State* state);
	static int hasMoreResults(lua_State* state);
	static int getNextResults(lua_State* state);
	int getData(lua_State* state);
	int dataReference = 0;
	std::string m_query;
};

class QueryData : public IQueryData {
	friend class Query;
public:
	my_ulonglong getLastInsertID() {
		return (m_insertIds.size() == 0) ? 0 : m_insertIds.front();
	}

	my_ulonglong getAffectedRows() {
		return (m_affectedRows.size() == 0) ? 0 : m_affectedRows.front();
	}

	bool hasMoreResults() {
		return m_insertIds.size() > 0 && m_affectedRows.size() > 0 && m_results.size() > 0;
	}

	bool getNextResults() {
		if (!hasMoreResults()) return false;
		m_results.pop_front();
		m_insertIds.pop_front();
		m_affectedRows.pop_front();
		return true;
	}

	ResultData& getResult() {
		return m_results.front();
	}

	std::deque<ResultData> getResults() {
		return m_results;
	}
protected:
	std::deque<my_ulonglong> m_affectedRows;
	std::deque<my_ulonglong> m_insertIds;
	std::deque<ResultData> m_results;
};
#endif