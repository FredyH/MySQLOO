#ifndef PREPAREDQUERY_
#define PREPAREDQUERY_

#include <unordered_map>
#include "Query.h"
#include "MySQLHeader.h"
#include "StatementHandle.h"
#include <sstream>

class PreparedQueryField {
    friend class PreparedQuery;

public:
    PreparedQueryField(unsigned int index, int type) : m_index(index), m_type(type) {}

    PreparedQueryField() : m_index(1), m_type(0) {}

    virtual ~PreparedQueryField() = default;

private:
    unsigned int m_index;
    int m_type;
};

template<typename T>
class TypedQueryField : public PreparedQueryField {
    friend class PreparedQuery;

public:
    TypedQueryField(unsigned int index, int type, const T &data)
            : PreparedQueryField(index, type), m_data(data) {};

    ~TypedQueryField() override = default;

private:
    T m_data;
};

class PreparedQueryData : public QueryData {
    friend class PreparedQuery;

protected:
    std::deque<std::unordered_map<unsigned int, std::shared_ptr<PreparedQueryField>>> m_parameters;
    bool firstAttempt = true;

    PreparedQueryData() = default;
};

class PreparedQuery : public Query {
    friend class Database;

public:
    ~PreparedQuery() override;

    void executeStatement(Database &database, MYSQL *connection, const std::shared_ptr<IQueryData> &data) override;

    void clearParameters();

    void setNumber(unsigned int index, double value);

    void setString(unsigned int index, const std::string &value);

    void setBoolean(unsigned int index, bool value);

    void setNull(unsigned int index);

    void putNewParameters();

    std::shared_ptr<QueryData> buildQueryData() override;

    static std::shared_ptr<PreparedQuery> create(const std::shared_ptr<Database> &dbase, std::string query);

private:
    PreparedQuery(const std::shared_ptr<Database> &dbase, std::string query);

    std::deque<std::unordered_map<unsigned int, std::shared_ptr<PreparedQueryField>>> m_parameters{};

    static MYSQL_STMT *mysqlStmtInit(MYSQL *sql);

    static void
    generateMysqlBinds(MYSQL_BIND *binds, std::unordered_map<unsigned int, std::shared_ptr<PreparedQueryField>> &map,
                       unsigned int parameterCount);

    static void mysqlStmtBindParameter(MYSQL_STMT *sql, MYSQL_BIND *bind);

    static void mysqlStmtPrepare(MYSQL_STMT *sql, const char *str);

    static void mysqlStmtExecute(MYSQL_STMT *sql);

    static void mysqlStmtStoreResult(MYSQL_STMT *sql);

    static bool mysqlStmtNextResult(MYSQL_STMT *sql);

    std::shared_ptr<StatementHandle> cachedStatement{nullptr};
};

#endif