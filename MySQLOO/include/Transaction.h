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
	Transaction(Database* dbase, GarrysMod::Lua::ILuaBase* LUA);
	void doCallback(GarrysMod::Lua::ILuaBase* LUA, std::shared_ptr<IQueryData> data);
	virtual std::shared_ptr<IQueryData> buildQueryData(GarrysMod::Lua::ILuaBase* LUA);
protected:
	static int clearQueries(lua_State* state);
	static int addQuery(lua_State* state);
	static int getQueries(lua_State* state);
	bool executeStatement(MYSQL * connection, std::shared_ptr<IQueryData> data);
	void onDestroyed(GarrysMod::Lua::ILuaBase* LUA);
};

class TransactionData : public IQueryData {
	friend class Transaction;
protected:
	std::deque<std::pair<std::shared_ptr<Query>, std::shared_ptr<IQueryData>>> m_queries;
	bool retried = false;
};
#endif
