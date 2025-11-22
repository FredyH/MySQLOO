#ifndef TRANSACTION_
#define TRANSACTION_

#include <mutex>
#include <atomic>
#include <condition_variable>
#include <utility>
#include <iostream>
#include "IQuery.h"
#include "Query.h"


class TransactionData : public IQueryData {
    friend class Transaction;

public:
    std::deque<std::pair<std::shared_ptr<Query>, std::shared_ptr<IQueryData>>> m_queries;

    void finishLuaQueryData(GarrysMod::Lua::ILuaBase *LUA, const std::shared_ptr<IQuery> &transaction) override;
protected:
    explicit TransactionData(std::deque<std::pair<std::shared_ptr<Query>, std::shared_ptr<IQueryData>>> queries) :
            m_queries(std::move(queries)) {
    };
};

class Transaction : public IQuery {
    friend class Database;

public:
    static std::shared_ptr<TransactionData>
    buildQueryData(const std::deque<std::pair<std::shared_ptr<Query>, std::shared_ptr<IQueryData>>> &queries);

    static std::shared_ptr<Transaction> create(const std::shared_ptr<Database> &database);

    std::string getSQLString() override { return ""; };

protected:
    void executeStatement(Database &database, MYSQL *connection, const std::shared_ptr<IQueryData> &data) override;

    explicit Transaction(const std::shared_ptr<Database> &database) : IQuery(database) {

    }

private:
    static void mysqlAutocommit(MYSQL *sql, bool auto_mode);

    static void mysqlCommit(MYSQL *sql);
};

#endif
