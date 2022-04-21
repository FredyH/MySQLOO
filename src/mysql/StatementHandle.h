#ifndef MYSQLOO_STATEMENTHANDLE_H
#define MYSQLOO_STATEMENTHANDLE_H

#include "mysql.h"

class StatementHandle {
public:
    StatementHandle(MYSQL_STMT *stmt, bool valid);

    MYSQL_STMT *stmt = nullptr;

    bool isValid() const { return stmt != nullptr && valid; };

    void invalidate() { valid = false; }

private:
    bool valid;
};

#endif //MYSQLOO_STATEMENTHANDLE_H
