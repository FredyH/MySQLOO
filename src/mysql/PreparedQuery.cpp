#include "PreparedQuery.h"

#include <memory>
#include "Database.h"
#include "errmsg.h"
#include "mysqld_error.h"
#include "MySQLOOException.h"

#ifdef LINUX
#include <stdlib.h>
#endif

PreparedQuery::PreparedQuery(const std::shared_ptr<Database> &dbase, std::string query) : Query(dbase,
                                                                                                std::move(query)) {
    putNewParameters();
}


PreparedQuery::~PreparedQuery() {
    m_database->freeStatement(cachedStatement);
}


void PreparedQuery::clearParameters() {
    m_parameters.clear();
    m_parameters.emplace_back();
}

void PreparedQuery::setNumber(unsigned int index, double value) {
    if (index < 1) {
        throw MySQLOOException("Index must be greater than 0");
    }
    m_parameters.back()[index] = std::shared_ptr<PreparedQueryField>(
            new TypedQueryField<double>(index, MYSQL_TYPE_DOUBLE, value));
}

void PreparedQuery::setString(unsigned int index, const std::string &value) {
    if (index < 1) {
        throw MySQLOOException("Index must be greater than 0");
    }
    m_parameters.back()[index] = std::shared_ptr<PreparedQueryField>(
            new TypedQueryField<std::string>(index, MYSQL_TYPE_STRING, value));
}

void PreparedQuery::setBoolean(unsigned int index, bool value) {
    if (index < 1) {
        throw MySQLOOException("Index must be greater than 0");
    }
    m_parameters.back()[index] = std::shared_ptr<PreparedQueryField>(
            new TypedQueryField<bool>(index, MYSQL_TYPE_BIT, value));
}

void PreparedQuery::setNull(unsigned int index) {
    if (index < 1) {
        throw MySQLOOException("Index must be greater than 0");
    }
    m_parameters.back()[index] = std::make_shared<PreparedQueryField>(index, MYSQL_TYPE_NULL);
}

//Adds an additional set of parameters to the prepared query
//This makes it relatively easy to insert multiple rows at once
void PreparedQuery::putNewParameters() {
    m_parameters.emplace_back();
}

//Wrapper functions that might throw errors
MYSQL_STMT *PreparedQuery::mysqlStmtInit(MYSQL *sql) {
    MYSQL_STMT *stmt = mysql_stmt_init(sql);
    if (stmt == nullptr) {
        const char *errorMessage = mysql_error(sql);
        unsigned int errorCode = mysql_errno(sql);
        throw MySQLException(errorCode, errorMessage);
    }
    return stmt;
}

void PreparedQuery::mysqlStmtBindParameter(MYSQL_STMT *stmt, MYSQL_BIND *bind) {
    my_bool result = mysql_stmt_bind_param(stmt, bind);
    if (result != 0) {
        const char *errorMessage = mysql_stmt_error(stmt);
        unsigned int errorCode = mysql_stmt_errno(stmt);
        throw MySQLException(errorCode, errorMessage);
    }
}

void PreparedQuery::mysqlStmtPrepare(MYSQL_STMT *stmt, const char *str) {
    auto length = (unsigned long) strlen(str);
    int result = mysql_stmt_prepare(stmt, str, length);
    if (result != 0) {
        const char *errorMessage = mysql_stmt_error(stmt);
        unsigned int errorCode = mysql_stmt_errno(stmt);
        throw MySQLException(errorCode, errorMessage);
    }
}

void PreparedQuery::mysqlStmtExecute(MYSQL_STMT *stmt) {
    int result = mysql_stmt_execute(stmt);
    if (result != 0) {
        const char *errorMessage = mysql_stmt_error(stmt);
        unsigned int errorCode = mysql_stmt_errno(stmt);
        throw MySQLException(errorCode, errorMessage);
    }
}

void PreparedQuery::mysqlStmtStoreResult(MYSQL_STMT *stmt) {
    int result = mysql_stmt_store_result(stmt);
    if (result != 0) {
        const char *errorMessage = mysql_stmt_error(stmt);
        unsigned int errorCode = mysql_stmt_errno(stmt);
        throw MySQLException(errorCode, errorMessage);
    }
}

bool PreparedQuery::mysqlStmtNextResult(MYSQL_STMT *stmt) {
    int result = mysql_stmt_next_result(stmt);
    if (result > 0) {
        const char *errorMessage = mysql_stmt_error(stmt);
        unsigned int errorCode = mysql_stmt_errno(stmt);
        throw MySQLException(errorCode, errorMessage);
    }
    return result == 0;
}

static my_bool nullBool = 1;
static int trueValue = 1;
static int falseValue = 0;

//Generates binds for a prepared query. In this case the binds are used to send the parameters to the server
void PreparedQuery::generateMysqlBinds(MYSQL_BIND *binds,
                                       std::unordered_map<unsigned int, std::shared_ptr<PreparedQueryField>> &map,
                                       unsigned int parameterCount) {
    for (unsigned int i = 1; i <= parameterCount; i++) {
        auto it = map.find(i);
        if (it == map.end()) {
            MYSQL_BIND *bind = &binds[i - 1];
            bind->buffer_type = MYSQL_TYPE_NULL;
            bind->is_null = &nullBool;
            continue;
        }
        unsigned int index = it->second->m_index - 1;
        if (index >= parameterCount) {
            std::stringstream errStream;
            errStream << "Invalid parameter index " << index + 1;
            throw MySQLException(0, errStream.str().c_str());
        }
        MYSQL_BIND *bind = &binds[index];
        switch (it->second->m_type) {
            case MYSQL_TYPE_DOUBLE: {
                auto *doubleField = dynamic_cast<TypedQueryField<double> *>(it->second.get());
                bind->buffer_type = MYSQL_TYPE_DOUBLE;
                bind->buffer = (char *) &doubleField->m_data;
                break;
            }
            case MYSQL_TYPE_BIT: {
                auto *boolField = dynamic_cast<TypedQueryField<bool> *>(it->second.get());
                bind->buffer_type = MYSQL_TYPE_LONG;
                bind->buffer = (char *) &((boolField->m_data) ? trueValue : falseValue);
                break;
            }
            case MYSQL_TYPE_STRING: {
                auto *textField = dynamic_cast<TypedQueryField<std::string> *>(it->second.get());
                bind->buffer_type = MYSQL_TYPE_STRING;
                bind->buffer = (char *) textField->m_data.c_str();
                bind->buffer_length = (unsigned long) textField->m_data.length();
                break;
            }
            case MYSQL_TYPE_NULL: {
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
void PreparedQuery::executeQuery(Database &database, MYSQL *connection, const std::shared_ptr<IQueryData> &ptr) {
    std::shared_ptr<PreparedQueryData> data = std::dynamic_pointer_cast<PreparedQueryData>(ptr);
    bool shouldReconnect = database.getAutoReconnect();
    //Autoreconnect has to be disabled for prepared statement since prepared statements
    //get reset on the server if the connection fails and auto reconnects
    try {
        MYSQL_STMT *stmt = nullptr;
        auto stmtClose = finally([&] {
            if (!database.shouldCachePreparedStatements() && stmt != nullptr) {
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
            if (database.shouldCachePreparedStatements()) {
                this->cachedStatement = stmt;
                database.cacheStatement(stmt);
            }
        }
        unsigned int parameterCount = mysql_stmt_param_count(stmt);
        std::vector<MYSQL_BIND> mysqlParameters(parameterCount);

        for (auto &currentMap: data->m_parameters) {
            generateMysqlBinds(mysqlParameters.data(), currentMap, parameterCount);
            mysqlStmtBindParameter(stmt, mysqlParameters.data());
            mysqlStmtExecute(stmt);
            do {
                data->m_affectedRows.push_back(mysql_stmt_affected_rows(stmt));
                data->m_insertIds.push_back(mysql_stmt_insert_id(stmt));
                data->m_resultStatus = QUERY_SUCCESS;

                MYSQL_RES *metaData = mysql_stmt_result_metadata(stmt);
                if (metaData == nullptr) {
                    //This means the statement does not have a resultset (this apparently happens when calling stored procedures)
                    //We need to skip this result, otherwise it screws up the mysql connection
                    //Add an empty ResultData in that case
                    //This is only necessary due to MariaDB client behaving differently to the Mysql client
                    //otherwise we get a hang in the rest of the code below
                    data->m_results.emplace_back();
                    continue;
                }
                auto f = finally([&] { mysql_free_result(metaData); });
                //There is a potential race condition here. What happens
                //when the query executes fine but something goes wrong while storing the result?
                mysqlStmtStoreResult(stmt);
                auto f2 = finally([&] { mysql_stmt_free_result(stmt); });
                data->m_results.emplace_back(stmt, metaData);
            } while (mysqlStmtNextResult(stmt));
        }
    } catch (const MySQLException &error) {
        unsigned int errorCode = error.getErrorCode();
        if (errorCode == CR_SERVER_LOST || errorCode == CR_SERVER_GONE_ERROR ||
            errorCode == ER_MAX_PREPARED_STMT_COUNT_REACHED || errorCode == CR_NO_PREPARE_STMT ||
            errorCode == ER_UNKNOWN_STMT_HANDLER) {
            database.freeStatement(this->cachedStatement);
            this->cachedStatement = nullptr;
            //Because autoreconnect is disabled we want to try and explicitly execute the prepared query once more
            //if we can get the client to reconnect (reconnect is caused by mysql_ping)
            //If this fails we just go ahead and error
            if (shouldReconnect && data->firstAttempt) {
                if (mysql_ping(connection) == 0) {
                    data->firstAttempt = false;
                    executeQuery(database, connection, ptr);
                    return;
                }
            }
        }
        //Rethrow error to be handled by executeStatement()
        throw error;
    }
}

bool PreparedQuery::executeStatement(Database &database, MYSQL *connection, std::shared_ptr<IQueryData> ptr) {
    std::shared_ptr<PreparedQueryData> data = std::dynamic_pointer_cast<PreparedQueryData>(ptr);
    data->setStatus(QUERY_RUNNING);
    try {
        this->executeQuery(database, connection, ptr);
        data->setResultStatus(QUERY_SUCCESS);
    } catch (const MySQLException &error) {
        data->setResultStatus(QUERY_ERROR);
        data->setError(error.what());
    }
    return true;
}

std::shared_ptr<QueryData> PreparedQuery::buildQueryData() {
    std::shared_ptr<PreparedQueryData> data(new PreparedQueryData());
    data->m_parameters = this->m_parameters;
    while (m_parameters.size() > 1) {
        //Front so the last used parameters are the ones that are gonna stay
        m_parameters.pop_front();
    }
    return std::dynamic_pointer_cast<QueryData>(data);
}

std::shared_ptr<PreparedQuery> PreparedQuery::create(const std::shared_ptr<Database> &dbase, std::string query) {
    return std::shared_ptr<PreparedQuery>(new PreparedQuery(dbase, std::move(query)));
}
