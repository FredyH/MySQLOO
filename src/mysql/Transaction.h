#ifndef TRANSACTION_
#define TRANSACTION_

#include <mutex>
#include <atomic>
#include <condition_variable>
#include <utility>
#include "IQuery.h"
#include "Query.h"


class TransactionData : public IQueryData {
    friend class Transaction;

public:
    std::deque<std::pair<std::shared_ptr<Query>, std::shared_ptr<IQueryData>>> m_queries;
protected:
    explicit TransactionData(std::deque<std::pair<std::shared_ptr<Query>, std::shared_ptr<IQueryData>>> queries) :
            m_queries(std::move(queries)) {
    };

    bool retried = false;
};

class Transaction : public IQuery {
    friend class Database;

public:

    static std::shared_ptr<TransactionData>
    buildQueryData(const std::deque<std::pair<std::shared_ptr<Query>, std::shared_ptr<IQueryData>>> &queries);

    static std::shared_ptr<Transaction> create(const std::weak_ptr<Database> &database);

protected:
    bool executeStatement(Database &database, MYSQL *connection, std::shared_ptr<IQueryData> data) override;

    explicit Transaction(const std::weak_ptr<Database> &database) : IQuery(database) {

    }

private:
    std::vector<std::shared_ptr<IQueryData>> addedQueryData;
};

#endif
