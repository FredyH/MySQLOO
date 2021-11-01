
#ifndef MYSQLOO_LUAPREPAREDQUERY_H
#define MYSQLOO_LUAPREPAREDQUERY_H


#include "LuaQuery.h"

class LuaPreparedQuery : public LuaQuery {
public:
    std::shared_ptr <IQueryData> buildQueryData() override;
    static void createMetaTable(ILuaBase *LUA);
};


#endif //MYSQLOO_LUAPREPAREDQUERY_H
