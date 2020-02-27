#include "PingQuery.h"
#ifdef LINUX
#include <stdlib.h>
#endif
#include "Database.h"


//Dummy class just used with the Database::ping function
PingQuery::PingQuery(Database* dbase, GarrysMod::Lua::ILuaBase* LUA) : Query(dbase, LUA) {
	classname = "PingQuery";
}

PingQuery::~PingQuery(void) {}

/* Executes the ping query
*/
void PingQuery::executeQuery(MYSQL* connection, std::shared_ptr<IQueryData> data) {
	bool oldAutoReconnect = this->m_database->getAutoReconnect();
	this->m_database->setAutoReconnect(true);
	this->pingSuccess = mysql_ping(connection) == 0;
	this->m_database->setAutoReconnect(oldAutoReconnect);
}