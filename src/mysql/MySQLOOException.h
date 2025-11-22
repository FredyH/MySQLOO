#ifndef MYSQLOO_MYSQLERROR_H
#define MYSQLOO_MYSQLERROR_H

#include <exception>

//Since MySQL 8.0.24, not available in the MariaDB client, so we've defined it here.
#define ER_CLIENT_INTERACTION_TIMEOUT 4031

//When called from the outside
class MySQLOOException : std::exception {
public:
    explicit MySQLOOException(std::string error) : std::exception(), message(std::move(error)) {
    }

    std::string message{};
};

#endif //MYSQLOO_MYSQLERROR_H
