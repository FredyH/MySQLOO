#pragma once
#ifndef PINGQUERY_
#define PINGQUERY_
#include <unordered_map>
#include "Query.h"
#include "MySQLHeader.h"
#include <sstream>
#include <string.h>


class PingQuery : public Query {
	friend class Database;
public:
	~PingQuery() override;
protected:
    explicit PingQuery(const std::weak_ptr<Database>& dbase);
	void executeQuery(Database &database, MYSQL* m_sql, std::shared_ptr<IQueryData> data) override;
	bool pingSuccess = false;
};
#endif