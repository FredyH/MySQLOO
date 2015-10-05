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
	bool executeStatement(MYSQL* connection);
};
#endif