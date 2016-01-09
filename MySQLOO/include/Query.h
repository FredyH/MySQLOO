#ifndef QUERY_
#define QUERY_
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "IQuery.h"


class Query : public IQuery
{
	friend class Database;
public:
	Query(Database* dbase, lua_State* state);
	virtual ~Query(void);
	void setQuery(std::string query);
	virtual bool executeStatement(MYSQL* m_sql);
	virtual void executeQuery(MYSQL* m_sql);
	void doCallback(lua_State* state);
protected:
	std::string m_query;
};
#endif