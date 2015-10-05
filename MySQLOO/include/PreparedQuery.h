#ifndef PREPAREDQUERY_
#define PREPAREDQUERY_
#include <unordered_map>
#include "IQuery.h"
#include "MySQLHeader.h"
#include <sstream>
#include <string.h>


class PreparedQueryField
{
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
class TypedQueryField : public PreparedQueryField
{
	friend class PreparedQuery;
public:
	TypedQueryField(unsigned int index, int type, const T& data)
		: PreparedQueryField(index, type), m_data(data){};
	virtual ~TypedQueryField() {}
private:
	T m_data;
};


class PreparedQuery : IQuery
{
	friend class Database;
public:
	PreparedQuery(Database* dbase, lua_State* state);
	virtual ~PreparedQuery(void);
	bool executeStatement(MYSQL* connection);
private:
	std::deque<std::unordered_map<unsigned int, std::unique_ptr<PreparedQueryField>>> parameters;
	static int setNumber(lua_State* state);
	static int setString(lua_State* state);
	static int setBoolean(lua_State* state);
	static int setNull(lua_State* state);
	static int putNewParameters(lua_State* state);
	MYSQL_STMT *mysqlStmtInit(MYSQL* sql);
	void generateMysqlBinds(MYSQL_BIND* binds, std::unordered_map<unsigned int, std::unique_ptr<PreparedQueryField>> *map, unsigned int parameterCount);
	void mysqlStmtBindParameter(MYSQL_STMT* sql, MYSQL_BIND* bind);
	void mysqlStmtPrepare(MYSQL_STMT* sql, const char* str);
	void mysqlStmtExecute(MYSQL_STMT* sql);
	void mysqlStmtStoreResult(MYSQL_STMT* sql);
};
#endif