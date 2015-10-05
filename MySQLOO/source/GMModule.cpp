#include "GarrysMod/Lua/Interface.h"
#include "LuaObjectBase.h"
#include "Database.h"
#include "Logger.h"
#include "MySQLHeader.h"
#include <iostream>
#include <iostream>
#include <fstream>
#define MYSQLOO_VERSION "9"

GMOD_MODULE_CLOSE()
{
	Logger::Log("-----------------------------\n");
	Logger::Log("MySQLOO closing\n");
	Logger::Log("-----------------------------\n");
	/* Deletes all the remaining luaobjects when the server changes map
	 */
	for (auto it = LuaObjectBase::luaRemovalObjects.begin(); it != LuaObjectBase::luaRemovalObjects.end(); ++it) {
		(*it)->onDestroyed(NULL);
	}
	LuaObjectBase::luaRemovalObjects.clear();
	LuaObjectBase::luaObjects.clear();
	LuaObjectBase::luaThinkObjects.clear();
	mysql_thread_end();
	return 0;
}

/* Connects to the database and returns a Database instance that can be used
 * as an interface to the mysql server.
 */
int connect(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	LUA->CheckType(1, GarrysMod::Lua::Type::STRING);
	LUA->CheckType(2, GarrysMod::Lua::Type::STRING);
	LUA->CheckType(3, GarrysMod::Lua::Type::STRING);
	LUA->CheckType(4, GarrysMod::Lua::Type::STRING);
	std::string host = LUA->GetString(1);
	std::string username = LUA->GetString(2);
	std::string pw = LUA->GetString(3);
	std::string database = LUA->GetString(4);
	unsigned int port = 3306;
	std::string unixSocket = "";
	if (LUA->IsType(5, GarrysMod::Lua::Type::NUMBER))
	{
		port = LUA->GetNumber(5);
	}
	if (LUA->IsType(6, GarrysMod::Lua::Type::STRING))
	{
		unixSocket = LUA->GetString(6);
	}
	Database* object = new Database(state, host, username, pw, database, port, unixSocket);
	((LuaObjectBase*) object)->pushTableReference(state);
	return 1;
}

GMOD_MODULE_OPEN()
{
	Logger::Log("-----------------------------\n");
	Logger::Log("MySQLOO starting\n");
	Logger::Log("-----------------------------\n");
	if (mysql_library_init(0, NULL, NULL)) {
		LUA->ThrowError("Could not initialize mysql library.");
	}
	LuaObjectBase::createMetatables(state);
	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->GetField(-1, "hook");
	LUA->GetField(-1, "Add");
	LUA->PushString("Think");
	LUA->PushString("__MySQLThinkHook");
	LUA->PushCFunction(LuaObjectBase::doThink);
	LUA->Call(3, 0);
	LUA->Pop();
	LUA->Pop();
	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		LUA->CreateTable();

		LUA->PushString(MYSQLOO_VERSION); LUA->SetField(-2, "VERSION");

			LUA->PushNumber(DATABASE_CONNECTED); LUA->SetField(-2, "DATABASE_CONNECTED");
			LUA->PushNumber(DATABASE_CONNECTING); LUA->SetField(-2, "DATABASE_CONNECTING");
			LUA->PushNumber(DATABASE_NOT_CONNECTED); LUA->SetField(-2, "DATABASE_NOT_CONNECTED");
			LUA->PushNumber(DATABASE_CONNECTION_FAILED); LUA->SetField(-2, "DATABASE_CONNECTION_FAILED");

			LUA->PushNumber(QUERY_NOT_RUNNING); LUA->SetField(-2, "QUERY_NOT_RUNNING");
			LUA->PushNumber(QUERY_RUNNING); LUA->SetField(-2, "QUERY_RUNNING");
			LUA->PushNumber(QUERY_COMPLETE); LUA->SetField(-2, "QUERY_COMPLETE");
			LUA->PushNumber(QUERY_ABORTED); LUA->SetField(-2, "QUERY_ABORTED");
			LUA->PushNumber(QUERY_WAITING); LUA->SetField(-2, "QUERY_WAITING");

			LUA->PushNumber(OPTION_NUMERIC_FIELDS); LUA->SetField(-2, "OPTION_NUMERIC_FIELDS");
			LUA->PushNumber(OPTION_NAMED_FIELDS); LUA->SetField(-2, "OPTION_NAMED_FIELDS"); //Not used anymore
			LUA->PushNumber(OPTION_CACHE); LUA->SetField(-2, "OPTION_CACHE"); //Not used anymore

			LUA->PushCFunction(connect); LUA->SetField(-2, "connect");

		LUA->SetField(-2, "mysqloo");
	LUA->Pop();
	return 1;
}