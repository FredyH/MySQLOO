#ifndef PREPAREDQUERY_
#define PREPAREDQUERY_
#include <unordered_map>
#include "Query.h"
#include "MySQLHeader.h"
#include <sstream>
#include <string.h>


class PreparedQueryField {
	friend class PreparedQuery;
public:
	PreparedQueryField(unsigned int index, int type) : m_index(index), m_type(type) {}
	PreparedQueryField() : m_index(1), m_type(0) {}
	virtual ~PreparedQueryField() {}
private:
	unsigned int m_index;
	int m_type;
};

template< typename T >
class TypedQueryField : public PreparedQueryField {
	friend class PreparedQuery;
public:
	TypedQueryField(unsigned int index, int type, const T& data)
		: PreparedQueryField(index, type), m_data(data) {};
	virtual ~TypedQueryField() {}
private:
	T m_data;
};


class PreparedQuery : public Query {
	friend class Database;
public:
	PreparedQuery(Database* dbase, GarrysMod::Lua::ILuaBase* LUA);
	virtual ~PreparedQuery(void);
	bool executeStatement(MYSQL* connection, std::shared_ptr<IQueryData> data);
	virtual void onDestroyed(GarrysMod::Lua::ILuaBase* LUA);
protected:
	virtual std::shared_ptr<IQueryData> buildQueryData(GarrysMod::Lua::ILuaBase* LUA);
	void executeQuery(MYSQL* m_sql, std::shared_ptr<IQueryData> data);
private:
	std::deque<std::unordered_map<unsigned int, std::shared_ptr<PreparedQueryField>>> m_parameters;
	static int clearParameters(lua_State* state);
	static int setNumber(lua_State* state);
	static int setString(lua_State* state);
	static int setBoolean(lua_State* state);
	static int setNull(lua_State* state);
	static int putNewParameters(lua_State* state);
	MYSQL_STMT *mysqlStmtInit(MYSQL* sql);
	void generateMysqlBinds(MYSQL_BIND* binds, std::unordered_map<unsigned int, std::shared_ptr<PreparedQueryField>> &map, unsigned int parameterCount);
	void mysqlStmtBindParameter(MYSQL_STMT* sql, MYSQL_BIND* bind);
	void mysqlStmtPrepare(MYSQL_STMT* sql, const char* str);
	void mysqlStmtExecute(MYSQL_STMT* sql);
	void mysqlStmtStoreResult(MYSQL_STMT* sql);
	bool mysqlStmtNextResult(MYSQL_STMT* sql);
	//This is atomic to prevent visibility issues
	std::atomic<MYSQL_STMT*> cachedStatement{ nullptr };
	//This pointer is used to prevent the database being accessed after it was deleted
	//when this preparedq query still owns a MYSQL_STMT*
	std::weak_ptr<Database> weak_database;
};

class PreparedQueryData : public QueryData {
	friend class PreparedQuery;
protected:
	std::deque<std::unordered_map<unsigned int, std::shared_ptr<PreparedQueryField>>> m_parameters;
	bool firstAttempt = true;
};
#endif