#ifndef QUERY_
#define QUERY_

#include <mutex>
#include <atomic>
#include <deque>
#include <condition_variable>
#include "IQuery.h"
#include "ResultData.h"

class QueryData;

class Query : public IQuery {
    friend class Database;

    friend class Transaction;

public:
    ~Query() override;

    bool executeStatement(Database &database, MYSQL *m_sql, std::shared_ptr<IQueryData> data) override;

    virtual void executeQuery(Database &database, MYSQL *m_sql, std::shared_ptr<IQueryData> data);

    my_ulonglong lastInsert();

    my_ulonglong affectedRows();

    bool hasMoreResults();

    void getNextResults();

    static std::shared_ptr<QueryData> buildQueryData();
protected:
    Query(const std::weak_ptr<Database> &dbase, std::string query);

    std::string m_query;
};

class QueryData : public IQueryData {
    friend class Query;

public:
    my_ulonglong getLastInsertID() {
        return (m_insertIds.empty()) ? 0 : m_insertIds.front();
    }

    my_ulonglong getAffectedRows() {
        return (m_affectedRows.empty()) ? 0 : m_affectedRows.front();
    }

    bool hasMoreResults() {
        return !m_insertIds.empty() && !m_affectedRows.empty() && !m_results.empty();
    }

    bool getNextResults() {
        if (!hasMoreResults()) return false;
        m_results.pop_front();
        m_insertIds.pop_front();
        m_affectedRows.pop_front();
        return true;
    }

    ResultData &getResult() {
        return m_results.front();
    }

    std::deque<ResultData> getResults() {
        return m_results;
    }

protected:
    std::deque<my_ulonglong> m_affectedRows;
    std::deque<my_ulonglong> m_insertIds;
    std::deque<ResultData> m_results;
    QueryData() = default;
};

#endif