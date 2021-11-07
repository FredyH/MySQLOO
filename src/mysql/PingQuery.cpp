#include "PingQuery.h"

#ifdef LINUX
#include <stdlib.h>
#endif

#include "Database.h"

//Dummy class just used with the Database::ping function
PingQuery::PingQuery(const std::shared_ptr<Database> &dbase) : Query(dbase, "") {

}

PingQuery::~PingQuery() = default;

/* Executes the ping query
*/
void PingQuery::executeQuery(Database &database, MYSQL *connection, const std::shared_ptr<IQueryData> &data) {
    bool oldAutoReconnect = database.getAutoReconnect();
    database.setAutoReconnect(true);
    this->pingSuccess = mysql_ping(connection) == 0;
    database.setAutoReconnect(oldAutoReconnect);
}