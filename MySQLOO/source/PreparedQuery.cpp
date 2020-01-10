#include "PreparedQuery.h"
#include "Database.h"
#include "errmsg.h"
#ifdef LINUX
#include <stdlib.h>
#endif
//This is dirty but hopefully will be consistent between mysql connector versions
#define ER_MAX_PREPARED_STMT_COUNT_REACHED 1461

PreparedQuery::PreparedQuery(Database* dbase, GarrysMod::Lua::ILuaBase* LUA) : Query(dbase, LUA) {
	classname = "PreparedQuery";
	registerFunction(LUA, "setNumber", PreparedQuery::setNumber);
	registerFunction(LUA, "setString", PreparedQuery::setString);
	registerFunction(LUA, "setBoolean", PreparedQuery::setBoolean);
	registerFunction(LUA, "setNull", PreparedQuery::setNull);
	registerFunction(LUA, "putNewParameters", PreparedQuery::putNewParameters);
	registerFunction(LUA, "clearParameters", PreparedQuery::clearParameters);
	this->m_parameters.push_back(std::unordered_map<unsigned int, std::shared_ptr<PreparedQueryField>>());
	//This pointer is used to prevent the database being accessed after it was deleted
	//when this preparedq query still owns a MYSQL_STMT*
	this->weak_database = std::dynamic_pointer_cast<Database>(m_database->getSharedPointerInstance());
}

PreparedQuery::~PreparedQuery(void) {}

//When the query is destroyed by lua
void PreparedQuery::onDestroyed(GarrysMod::Lua::ILuaBase* LUA) {
	//There can't be any race conditions here
	//This always runs after PreparedQuery::executeQuery() is done
	//I am using atomic to prevent visibility issues though
	auto ptr = this->weak_database.lock();
	if (ptr.get() != nullptr) {
		MYSQL_STMT* stmt = this->cachedStatement;
		if (stmt != nullptr) {
			ptr->freeStatement(cachedStatement);
			cachedStatement = nullptr;
		}
	}
	IQuery::onDestroyed(LUA);
}

int PreparedQuery::clearParameters(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	PreparedQuery* object = (PreparedQuery*)unpackSelf(LUA, TYPE_QUERY);
	object->m_parameters.clear();
	object->m_parameters.emplace_back();
	return 0;
}

int PreparedQuery::setNumber(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	PreparedQuery* object = (PreparedQuery*)unpackSelf(LUA, TYPE_QUERY);
	LUA->CheckType(2, GarrysMod::Lua::Type::NUMBER);
	LUA->CheckType(3, GarrysMod::Lua::Type::NUMBER);
	double index = LUA->GetNumber(2);
	if (index < 1) LUA->ThrowError("Index must be greater than 0");
	unsigned int uIndex = (unsigned int)index;
	double value = LUA->GetNumber(3);
	object->m_parameters.back()[uIndex] = std::shared_ptr<PreparedQueryField>(new TypedQueryField<double>(uIndex, MYSQL_TYPE_DOUBLE, value));
	return 0;
}

int PreparedQuery::setString(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	PreparedQuery* object = (PreparedQuery*)unpackSelf(LUA, TYPE_QUERY);
	LUA->CheckType(2, GarrysMod::Lua::Type::NUMBER);
	LUA->CheckType(3, GarrysMod::Lua::Type::STRING);
	double index = LUA->GetNumber(2);
	if (index < 1) LUA->ThrowError("Index must be greater than 0");
	unsigned int uIndex = (unsigned int)index;
	unsigned int length = 0;
	const char* string = LUA->GetString(3, &length);
	object->m_parameters.back()[uIndex] = std::shared_ptr<PreparedQueryField>(new TypedQueryField<std::string>(uIndex, MYSQL_TYPE_STRING, std::string(string, length)));
	return 0;
}

int PreparedQuery::setBoolean(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	PreparedQuery* object = (PreparedQuery*)unpackSelf(LUA, TYPE_QUERY);
	LUA->CheckType(2, GarrysMod::Lua::Type::NUMBER);
	LUA->CheckType(3, GarrysMod::Lua::Type::BOOL);
	double index = LUA->GetNumber(2);
	if (index < 1) LUA->ThrowError("Index must be greater than 0");
	unsigned int uIndex = (unsigned int)index;
	bool value = LUA->GetBool(3);
	object->m_parameters.back()[uIndex] = std::shared_ptr<PreparedQueryField>(new TypedQueryField<bool>(uIndex, MYSQL_TYPE_BIT, value));
	return 0;
}

int PreparedQuery::setNull(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	PreparedQuery* object = (PreparedQuery*)unpackSelf(LUA, TYPE_QUERY);
	LUA->CheckType(2, GarrysMod::Lua::Type::NUMBER);
	double index = LUA->GetNumber(2);
	if (index < 1) LUA->ThrowError("Index must be greater than 0");
	unsigned int uIndex = (unsigned int)index;
	object->m_parameters.back()[uIndex] = std::shared_ptr<PreparedQueryField>(new PreparedQueryField(uIndex, MYSQL_TYPE_NULL));
	return 0;
}

//Adds an additional set of parameters to the prepared query
//This makes it relatively easy to insert multiple rows at once
int PreparedQuery::putNewParameters(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	PreparedQuery* object = (PreparedQuery*)unpackSelf(LUA, TYPE_QUERY);
	object->m_parameters.emplace_back();
	return 0;
}

//Wrapper functions that might throw errors
MYSQL_STMT* PreparedQuery::mysqlStmtInit(MYSQL* sql) {
	MYSQL_STMT* stmt = mysql_stmt_init(sql);
	if (stmt == nullptr) {
		const char* errorMessage = mysql_error(sql);
		int errorCode = mysql_errno(sql);
		throw MySQLException(errorCode, errorMessage);
	}
	return stmt;
}

void PreparedQuery::mysqlStmtBindParameter(MYSQL_STMT* stmt, MYSQL_BIND* bind) {
	int result = mysql_stmt_bind_param(stmt, bind);
	if (result != 0) {
		const char* errorMessage = mysql_stmt_error(stmt);
		int errorCode = mysql_stmt_errno(stmt);
		throw MySQLException(errorCode, errorMessage);
	}
}

void PreparedQuery::mysqlStmtPrepare(MYSQL_STMT* stmt, const char* str) {
	unsigned int length = strlen(str);
	int result = mysql_stmt_prepare(stmt, str, length);
	if (result != 0) {
		const char* errorMessage = mysql_stmt_error(stmt);
		int errorCode = mysql_stmt_errno(stmt);
		throw MySQLException(errorCode, errorMessage);
	}
}

void PreparedQuery::mysqlStmtExecute(MYSQL_STMT* stmt) {
	int result = mysql_stmt_execute(stmt);
	if (result != 0) {
		const char* errorMessage = mysql_stmt_error(stmt);
		int errorCode = mysql_stmt_errno(stmt);
		throw MySQLException(errorCode, errorMessage);
	}
}

void PreparedQuery::mysqlStmtStoreResult(MYSQL_STMT* stmt) {
	int result = mysql_stmt_store_result(stmt);
	if (result != 0) {
		const char* errorMessage = mysql_stmt_error(stmt);
		int errorCode = mysql_stmt_errno(stmt);
		throw MySQLException(errorCode, errorMessage);
	}
}

bool PreparedQuery::mysqlStmtNextResult(MYSQL_STMT* stmt) {
	int result = mysql_stmt_next_result(stmt);
	if (result > 0) {
		const char* errorMessage = mysql_stmt_error(stmt);
		int errorCode = mysql_stmt_errno(stmt);
		throw MySQLException(errorCode, errorMessage);
	}
	return result == 0;
}

static my_bool nullBool = 1;
static int trueValue = 1;
static int falseValue = 0;

//Generates binds for a prepared query. In this case the binds are used to send the parameters to the server
void PreparedQuery::generateMysqlBinds(MYSQL_BIND* binds, std::unordered_map<unsigned int, std::shared_ptr<PreparedQueryField>> &map, unsigned int parameterCount) {
	for (unsigned int i = 1; i <= parameterCount; i++) {
		auto it = map.find(i);
		if (it == map.end()) {
			MYSQL_BIND* bind = &binds[i - 1];
			bind->buffer_type = MYSQL_TYPE_NULL;
			bind->is_null = &nullBool;
			continue;
		}
		unsigned int index = it->second->m_index - 1;
		if (index >= parameterCount) {
			std::stringstream  errStream;
			errStream << "Invalid parameter index " << index + 1;
			throw MySQLException(0, errStream.str().c_str());
		}
		MYSQL_BIND* bind = &binds[index];
		switch (it->second->m_type) {
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
void PreparedQuery::executeQuery(MYSQL* connection, std::shared_ptr<IQueryData> ptr) {
	PreparedQueryData* data = (PreparedQueryData*)ptr.get();
	my_bool oldReconnectStatus = m_database->getAutoReconnect();
	//Autoreconnect has to be disabled for prepared statement since prepared statements
	//get reset on the server if the connection fails and auto reconnects
	m_database->setAutoReconnect((my_bool) 0);
	auto resetReconnectStatus = finally([&] { m_database->setAutoReconnect(oldReconnectStatus); });
	try {
		MYSQL_STMT* stmt = nullptr;
		auto stmtClose = finally([&] {
			if (!m_database->shouldCachePreparedStatements() && stmt != nullptr) {
				mysql_stmt_close(stmt);
			}
		});
		if (this->cachedStatement.load() != nullptr) {
			stmt = this->cachedStatement;
		} else {
			stmt = mysqlStmtInit(connection);
			my_bool attrMaxLength = 1;
			mysql_stmt_attr_set(stmt, STMT_ATTR_UPDATE_MAX_LENGTH, &attrMaxLength);
			mysqlStmtPrepare(stmt, this->m_query.c_str());
			if (m_database->shouldCachePreparedStatements()) {
				this->cachedStatement = stmt;
				m_database->cacheStatement(stmt);
			}
		}
		unsigned int parameterCount = mysql_stmt_param_count(stmt);
		std::vector<MYSQL_BIND> mysqlParameters(parameterCount);

		for (auto& currentMap : data->m_parameters) {
			generateMysqlBinds(mysqlParameters.data(), currentMap, parameterCount);
			mysqlStmtBindParameter(stmt, mysqlParameters.data());
			mysqlStmtExecute(stmt);
			do {
				//There is a potential race condition here. What happens
				//when the query executes fine but something goes wrong while storing the result?
				mysqlStmtStoreResult(stmt);
				auto resultFree = finally([&] { mysql_stmt_free_result(stmt); });
				data->m_affectedRows.push_back(mysql_stmt_affected_rows(stmt));
				data->m_insertIds.push_back(mysql_stmt_insert_id(stmt));
				data->m_results.emplace_back(stmt);
				data->m_resultStatus = QUERY_SUCCESS;
			} while (mysqlStmtNextResult(stmt));
		}
	} catch (const MySQLException& error) {
		int errorCode = error.getErrorCode();
		if ((errorCode == CR_SERVER_LOST || errorCode == CR_SERVER_GONE_ERROR || errorCode == ER_MAX_PREPARED_STMT_COUNT_REACHED)) {
			m_database->freeStatement(this->cachedStatement);
			this->cachedStatement = nullptr;
			//Because autoreconnect is disabled we want to try and explicitly execute the prepared query once more
			//if we can get the client to reconnect (reconnect is caused by mysql_ping)
			//If this fails we just go ahead and error
			if (oldReconnectStatus && data->firstAttempt) {
				m_database->setAutoReconnect((my_bool) 1);
				if (mysql_ping(connection) == 0) {
					data->firstAttempt = false;
					executeQuery(connection, ptr);
					return;
				}
			}
		}
		//Rethrow error to be handled by executeStatement()
		throw error;
	}
}

bool PreparedQuery::executeStatement(MYSQL* connection, std::shared_ptr<IQueryData> ptr) {
	PreparedQueryData* data = (PreparedQueryData*)ptr.get();
	data->setStatus(QUERY_RUNNING);
	try {
		this->executeQuery(connection, ptr);
		data->setResultStatus(QUERY_SUCCESS);
	} catch (const MySQLException& error) {
		data->setResultStatus(QUERY_ERROR);
		data->setError(error.what());
	}
	return true;
}


std::shared_ptr<IQueryData> PreparedQuery::buildQueryData(GarrysMod::Lua::ILuaBase* LUA) {
	std::shared_ptr<IQueryData> ptr(new PreparedQueryData());
	PreparedQueryData* data = (PreparedQueryData*)ptr.get();
	data->m_parameters = this->m_parameters;
	while (m_parameters.size() > 1) {
		//Front so the last used parameters are the ones that are gonna stay
		m_parameters.pop_front();
	}
	return ptr;
}