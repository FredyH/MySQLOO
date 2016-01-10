#ifndef TRANSACTION_
#define TRANSACTION_
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "IQuery.h"
#include "Query.h"


class Transaction : public IQuery
{
	friend class Database;
public:
	Transaction(Database* dbase, lua_State* state);
	void doCallback(lua_State* state);
protected:
	static int addQuery(lua_State* state);
	static int getQueries(lua_State* state);
	bool executeStatement(MYSQL * connection);
private:
	std::deque<Query*> queries;
	bool retried = false;
	std::mutex m_queryMutex;
};
#endif