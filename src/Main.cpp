#include <iostream>
#include <memory>
#include "mysql/Database.h"

static std::shared_ptr<Database> db;

int main() {
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
        auto& firstRow = firstResultSet.getRows().front();
        auto& firstValue = firstRow.getValues().front();
        std::cout << "Result: " << firstValue << std::endl;
    }
    //mysql_library_end();
}