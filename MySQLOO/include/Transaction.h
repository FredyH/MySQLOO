#ifndef TRANSACTION_
#define TRANSACTION_
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "IQuery.h"
#include "Query.h"


class Transaction : public IQuery {
	friend class Database;
public:
	Transaction(Database* dbase, lua_State* state);
	void doCallback(lua_State* state, std::shared_ptr<IQueryData> data);
	virtual std::shared_ptr<IQueryData> buildQueryData(lua_State* state);
protected:
	static int clearQueries(lua_State* state);
	static int addQuery(lua_State* state);
	static int getQueries(lua_State* state);
	bool executeStatement(MYSQL * connection, std::shared_ptr<IQueryData> data);
	void onDestroyed(lua_State* state);
};

class TransactionData : public IQueryData {
	friend class Transaction;
protected:
	std::deque<std::pair<std::shared_ptr<Query>, std::shared_ptr<IQueryData>>> m_queries;
	bool retried = false;
};
#endif
