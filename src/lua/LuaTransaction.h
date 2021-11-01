
#ifndef MYSQLOO_LUATRANSACTION_H
#define MYSQLOO_LUATRANSACTION_H

#include "LuaIQuery.h"

class LuaTransaction : public LuaIQuery {
public:
    std::deque<std::shared_ptr<QueryData>> m_addedQueryData = {};

    std::shared_ptr<IQueryData> buildQueryData() override;

    static void createMetaTable(ILuaBase *LUA);
};


#endif //MYSQLOO_LUATRANSACTION_H
