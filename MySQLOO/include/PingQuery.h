#pragma once
#ifndef PINGQUERY_
#define PINGQUERY_
#include <unordered_map>
#include "Query.h"
#include "MySQLHeader.h"
#include <sstream>
#include <string.h>


class PingQuery : Query {
	friend class Database;
public:
	PingQuery(Database* dbase, GarrysMod::Lua::ILuaBase* LUA);
	virtual ~PingQuery(void);
protected:
	void executeQuery(MYSQL* m_sql, std::shared_ptr<IQueryData>);
	bool pingSuccess = false;
};
#endif