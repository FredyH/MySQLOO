#ifndef MYSQLOO_MYSQLERROR_H
#define MYSQLOO_MYSQLERROR_H

#include <exception>

//When called from the outside
class MySQLOOException : std::exception {
public:
    explicit MySQLOOException(std::string error) : std::exception(), message(std::move(error)) {
    }

    std::string message{};
};

#endif //MYSQLOO_MYSQLERROR_H
