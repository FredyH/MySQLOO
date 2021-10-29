#include <iostream>
#include <memory>
#include "mysql/Database.h"

static std::shared_ptr<Database> db;

int main() {
    mysql_library_init(0, nullptr, nullptr);
    std::cout << "Test" << std::endl;
    db = Database::createDatabase("127.0.0.1", "root", "test", "mysql", 3306, "");
    db->connect();
    db->wait();
    std::cout << "DB Connected" << std::endl;
    std::cout << "Ping returned: " << db->ping() << std::endl;

    for (int i = 0; i < 100; i++) {
        auto query = db->prepare("SELECT ?");
        query->setNumber(1, 2.0);
        auto queryData = std::dynamic_pointer_cast<PreparedQueryData>(query->buildQueryData());
        query->start(queryData);
        query->wait(true);
        auto firstResultSet = queryData->getResult();
        auto &firstRow = firstResultSet.getRows().front();
        auto &firstValue = firstRow.getValues().front();
        std::cout << "Result: " << firstValue << std::endl;
    }

    auto transaction = db->transaction();
    auto transactionQuery1 = db->prepare("SELECT ?");
    transactionQuery1->setNumber(1, 3.0);
    auto transactionData1 = transactionQuery1->buildQueryData();
    auto transactionQuery2 = db->query("SELECT 12");
    auto transactionData2 = transactionQuery1->buildQueryData();
    std::deque<std::pair<std::shared_ptr<Query>, std::shared_ptr<IQueryData>>> transactionQueries;
    transactionQueries.emplace_back(transactionQuery1, transactionData1);
    transactionQueries.emplace_back(transactionQuery2, transactionData2);
    auto transactionData = transaction->buildQueryData(transactionQueries);
    transaction->start(transactionData);
    transaction->wait(true);
    auto firstResultSet = transactionData2->getResult();
    auto &firstRow = firstResultSet.getRows().front();
    auto &firstValue = firstRow.getValues().front();
    std::cout << "Transaction Result: " << firstValue << std::endl;

    mysql_library_end();
}