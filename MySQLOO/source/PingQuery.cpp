#include "PingQuery.h"
#include "Logger.h"
#ifdef LINUX
#include <stdlib.h>
#endif


//Dummy class just used with the Database::ping function
PingQuery::PingQuery(Database* dbase, lua_State* state) : Query(dbase, state)
{
	classname = "PingQuery";
}

PingQuery::~PingQuery(void)
{
}

/* Executes the ping query
*/
void PingQuery::executeQuery(MYSQL* connection)
{
	LOG_CURRENT_FUNCTIONCALL
	this->pingSuccess = mysql_ping(connection) == 0;
}