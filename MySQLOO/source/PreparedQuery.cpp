#include "PreparedQuery.h"
#include "Logger.h"
#ifdef LINUX
#include <stdlib.h>
#endif

PreparedQuery::PreparedQuery(Database* dbase, lua_State* state) : Query(dbase, state)
{
	classname = "PreparedQuery";
	registerFunction(state, "setNumber", PreparedQuery::setNumber);
	registerFunction(state, "setString", PreparedQuery::setString);
	registerFunction(state, "setBoolean", PreparedQuery::setBoolean);
	registerFunction(state, "setNull", PreparedQuery::setNull);
	registerFunction(state, "putNewParameters", PreparedQuery::putNewParameters);
	this->parameters.push_back(std::unordered_map<unsigned int, std::unique_ptr<PreparedQueryField>>());
}

PreparedQuery::~PreparedQuery(void)
{
}

int PreparedQuery::setNumber(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	PreparedQuery* object = (PreparedQuery*)unpackSelf(state, TYPE_QUERY);
	if (object->m_status != QUERY_NONE)
		LUA->ThrowError("Query already started");
	LUA->CheckType(2, GarrysMod::Lua::Type::NUMBER);
	LUA->CheckType(3, GarrysMod::Lua::Type::NUMBER);
	double index = LUA->GetNumber(2);
	if (index < 1) LUA->ThrowError("Index must be greater than 0");
	unsigned int uIndex = (unsigned int) index;
	double value = LUA->GetNumber(3);
	object->parameters.back().insert(std::make_pair(uIndex, std::unique_ptr<PreparedQueryField>(new TypedQueryField<double>(uIndex, MYSQL_TYPE_DOUBLE, value))));
	return 0;
}

int PreparedQuery::setString(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	PreparedQuery* object = (PreparedQuery*)unpackSelf(state, TYPE_QUERY);
	if (object->m_status != QUERY_NONE)
		LUA->ThrowError("Query already started");
	LUA->CheckType(2, GarrysMod::Lua::Type::NUMBER);
	LUA->CheckType(3, GarrysMod::Lua::Type::STRING);
	double index = LUA->GetNumber(2);
	if (index < 1) LUA->ThrowError("Index must be greater than 0");
	unsigned int uIndex = (unsigned int) index;
	unsigned int length = 0;
	const char* string = LUA->GetString(3, &length);
	object->parameters.back().insert(std::make_pair(uIndex, std::unique_ptr<PreparedQueryField>(new TypedQueryField<std::string>(uIndex, MYSQL_TYPE_STRING, std::string(string, length)))));
	return 0;
}

int PreparedQuery::setBoolean(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	PreparedQuery* object = (PreparedQuery*)unpackSelf(state, TYPE_QUERY);
	if (object->m_status != QUERY_NONE)
		LUA->ThrowError("Query already started");
	LUA->CheckType(2, GarrysMod::Lua::Type::NUMBER);
	LUA->CheckType(3, GarrysMod::Lua::Type::BOOL);
	double index = LUA->GetNumber(2);
	if (index < 1) LUA->ThrowError("Index must be greater than 0");
	unsigned int uIndex = (unsigned int) index;
	bool value = LUA->GetBool(3);
	object->parameters.back().insert(std::make_pair(uIndex, std::unique_ptr<PreparedQueryField>(new TypedQueryField<bool>(uIndex, MYSQL_TYPE_BIT, value))));
	return 0;
}

int PreparedQuery::setNull(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	PreparedQuery* object = (PreparedQuery*)unpackSelf(state, TYPE_QUERY);
	if (object->m_status != QUERY_NONE)
		LUA->ThrowError("Query already started");
	LUA->CheckType(2, GarrysMod::Lua::Type::NUMBER);
	double index = LUA->GetNumber(2);
	if (index < 1) LUA->ThrowError("Index must be greater than 0");
	unsigned int uIndex = (unsigned int) index;
	object->parameters.back().insert(std::make_pair(uIndex, std::unique_ptr<PreparedQueryField>(new PreparedQueryField(uIndex, MYSQL_TYPE_NULL))));
	return 0;
}

//Adds an additional set of parameters to the prepared query
//This makes it relatively easy to insert multiple rows at once
int PreparedQuery::putNewParameters(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	PreparedQuery* object = (PreparedQuery*)unpackSelf(state, TYPE_QUERY);
	if (object->m_status != QUERY_NONE)
		LUA->ThrowError("Query already started");
	object->parameters.push_back(std::unordered_map<unsigned int, std::unique_ptr<PreparedQueryField>>());
	return 0;
}

//Wrapper functions that might throw errors
MYSQL_STMT* PreparedQuery::mysqlStmtInit(MYSQL* sql)
{
	MYSQL_STMT* stmt = mysql_stmt_init(sql);
	if (stmt == nullptr)
	{
		const char* errorMessage = mysql_error(sql);
		int errorCode = mysql_errno(sql);
		throw MySQLException(errorCode, errorMessage);
	}
	return stmt;
}

void PreparedQuery::mysqlStmtBindParameter(MYSQL_STMT* stmt, MYSQL_BIND* bind)
{
	int result = mysql_stmt_bind_param(stmt, bind);
	if (result != 0)
	{
		const char* errorMessage = mysql_stmt_error(stmt);
		int errorCode = mysql_stmt_errno(stmt);
		throw MySQLException(errorCode, errorMessage);
	}
}

void PreparedQuery::mysqlStmtPrepare(MYSQL_STMT* stmt, const char* str)
{
	unsigned int length = strlen(str);
	int result = mysql_stmt_prepare(stmt, str, length);
	if (result != 0)
	{
		const char* errorMessage = mysql_stmt_error(stmt);
		int errorCode = mysql_stmt_errno(stmt);
		throw MySQLException(errorCode, errorMessage);
	}
}

void PreparedQuery::mysqlStmtExecute(MYSQL_STMT* stmt)
{
	int result = mysql_stmt_execute(stmt);
	if (result != 0)
	{
		const char* errorMessage = mysql_stmt_error(stmt);
		int errorCode = mysql_stmt_errno(stmt);
		throw MySQLException(errorCode, errorMessage);
	}
}

void PreparedQuery::mysqlStmtStoreResult(MYSQL_STMT* stmt)
{
	int result = mysql_stmt_store_result(stmt);
	if (result != 0)
	{
		const char* errorMessage = mysql_stmt_error(stmt);
		int errorCode = mysql_stmt_errno(stmt);
		throw MySQLException(errorCode, errorMessage);
	}
}
static my_bool nullBool = 1;
static int trueValue = 1;
static int falseValue = 0;

//Generates binds for a prepared query. In this case the binds are used to send the parameters to the server
void PreparedQuery::generateMysqlBinds(MYSQL_BIND* binds, std::unordered_map<unsigned int, std::unique_ptr<PreparedQueryField>> *map, unsigned int parameterCount)
{
	LOG_CURRENT_FUNCTIONCALL
	for (unsigned int i = 1; i <= parameterCount; i++)
	{
		auto it = map->find(i);
		if (it == map->end())
		{
			MYSQL_BIND* bind = &binds[i-1];
			bind->buffer_type = MYSQL_TYPE_NULL;
			bind->is_null = &nullBool;
			continue;
		}
		unsigned int index = it->second->m_index - 1;
		if (index >= parameterCount)
		{
			std::stringstream  errStream;
			errStream << "Invalid parameter index " << index + 1;
			throw MySQLException(0, errStream.str().c_str());
		}
		MYSQL_BIND* bind = &binds[index];
		switch (it->second->m_type)
		{
			case MYSQL_TYPE_DOUBLE:
			{
				TypedQueryField<double>* doubleField = static_cast<TypedQueryField<double>*>(it->second.get());
				bind->buffer_type = MYSQL_TYPE_DOUBLE;
				bind->buffer = (char*)&doubleField->m_data;
				break;
			}
			case MYSQL_TYPE_BIT:
			{
				TypedQueryField<bool>* boolField = static_cast<TypedQueryField<bool>*>(it->second.get());
				bind->buffer_type = MYSQL_TYPE_LONG;
				bind->buffer = (char*)& ((boolField->m_data) ? trueValue : falseValue);
				break;
			}
			case MYSQL_TYPE_STRING:
			{
				TypedQueryField<std::string>* textField = static_cast<TypedQueryField<std::string>*>(it->second.get());
				bind->buffer_type = MYSQL_TYPE_STRING;
				bind->buffer = (char*)textField->m_data.c_str();
				bind->buffer_length = textField->m_data.length();
				break;
			}
			case MYSQL_TYPE_NULL:
			{
				bind->buffer_type = MYSQL_TYPE_NULL;
				bind->is_null = &nullBool;
				break;
			}
		}
	}
}



/* Executes the prepared query
* This function can only ever return one result set
* Note: If an error occurs at the nth query all the actions done before
* that nth query won't be reverted even though this query results in an error
*/
void PreparedQuery::executeQuery(MYSQL* connection)
{
	MYSQL_STMT* stmt = mysqlStmtInit(connection);
	my_bool attrMaxLength = 1;
	mysql_stmt_attr_set(stmt, STMT_ATTR_UPDATE_MAX_LENGTH, &attrMaxLength);
	mysqlStmtPrepare(stmt, this->m_query.c_str());
	auto queryFree = finally([&] {
		if (stmt != nullptr) {
			mysql_stmt_close(stmt);
			stmt = nullptr;
		}
		this->parameters.clear();
	});
	unsigned int parameterCount = mysql_stmt_param_count(stmt);
	std::vector<MYSQL_BIND> mysqlParameters(parameterCount);

	for (auto& currentMap : parameters)
	{
		generateMysqlBinds(mysqlParameters.data(), &currentMap, parameterCount);
		mysqlStmtBindParameter(stmt, mysqlParameters.data());
		mysqlStmtExecute(stmt);
		mysqlStmtStoreResult(stmt);
		auto resultFree = finally([&] { mysql_stmt_free_result(stmt); });
		this->results.emplace_back(stmt);
		this->m_affectedRows.push_back(mysql_stmt_affected_rows(stmt));
		this->m_insertIds.push_back(mysql_stmt_insert_id(stmt));
		this->m_resultStatus = QUERY_SUCCESS;
		//This is used to clear the connection in case there are
		//more ResultSets from a Procedure
		while (this->mysqlNextResult(connection))
		{
			MYSQL_RES * result = this->mysqlStoreResults(connection);
			mysql_free_result(result);
		}
	}
}

bool PreparedQuery::executeStatement(MYSQL* connection)
{
	LOG_CURRENT_FUNCTIONCALL
	this->m_status = QUERY_RUNNING;
	try
	{
		this->executeQuery(connection);
		this->m_resultStatus = QUERY_SUCCESS;
	}
	catch (const MySQLException& error)
	{
		this->m_resultStatus = QUERY_ERROR;
		this->m_errorText = error.what();
	}
	return true;
}