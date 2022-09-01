#pragma once
#ifndef PINGQUERY_
#define PINGQUERY_

#include <unordered_map>
#include "Query.h"
#include "MySQLHeader.h"
#include <sstream>
#include <cstring>


class PingQuery : public Query {
    friend class Database;

public:
    ~PingQuery() override;

protected:
    explicit PingQuery(const std::shared_ptr<Database> &dbase);

    std::string getSQLString() override { return ""; };

    void executeStatement(Database &database, MYSQL *m_sql, const std::shared_ptr<IQueryData> &data) override;

    bool pingSuccess = false;
};

#endif