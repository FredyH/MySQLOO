
#include "LuaDatabase.h"
#include "LuaQuery.h"
#include "LuaPreparedQuery.h"
#include "LuaTransaction.h"

static void pushLuaObjectTable(ILuaBase *LUA, void *data, int type) {
    LUA->CreateTable();
    LUA->PushUserType(data, LuaObject::TYPE_USERDATA);
    LUA->SetField(-2, "__CppObject");
    LUA->PushMetaTable(type);
    LUA->SetMetaTable(-2);
}

LUA_CLASS_FUNCTION(LuaDatabase, create) {
    LUA->CheckType(1, GarrysMod::Lua::Type::String);
    LUA->CheckType(2, GarrysMod::Lua::Type::String);
    LUA->CheckType(3, GarrysMod::Lua::Type::String);
    LUA->CheckType(4, GarrysMod::Lua::Type::String);
    std::string host = LUA->GetString(1);
    std::string username = LUA->GetString(2);
    std::string pw = LUA->GetString(3);
    std::string database = LUA->GetString(4);
    unsigned int port = 3306;
    std::string unixSocket;
    if (LUA->IsType(5, GarrysMod::Lua::Type::Number)) {
        port = (int) LUA->GetNumber(5);
    }
    if (LUA->IsType(6, GarrysMod::Lua::Type::String)) {
        unixSocket = LUA->GetString(6);
    }
    auto createdDatabase = Database::createDatabase(host, username, pw, database, port, unixSocket);
    auto luaDatabase = new LuaDatabase(createdDatabase);

    pushLuaObjectTable(LUA, luaDatabase, LuaObject::TYPE_DATABASE);
    int databaseTablePos = LUA->Top();

    //Add database to weak database table
    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->GetField(-1, "mysqloo");
    LUA->GetField(-1, "__weakDatabases");
    if (LUA->IsType(-1, GarrysMod::Lua::Type::Nil)) {
        LUA->Pop(3); //nil, mysqloo, Global
        return 1;
    }
    LUA->Push(databaseTablePos);
    LUA->PushBool(true);
    LUA->RawSet(-3);
    LUA->Pop(3); //__weakDatabases, mysqloo, Global

    return 1;
}

MYSQLOO_LUA_FUNCTION(query) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    LUA->CheckType(2, GarrysMod::Lua::Type::String);

    unsigned int outLen = 0;
    const char *queryStr = LUA->GetString(2, &outLen);
    auto query = Query::create(database->m_database, std::string(queryStr, outLen));

    LUA->Push(1);
    int databaseRef = LuaReferenceCreate(LUA);

    auto luaQuery = new LuaQuery(query, databaseRef);

    pushLuaObjectTable(LUA, luaQuery, LuaObject::TYPE_QUERY);
    return 1;
}

MYSQLOO_LUA_FUNCTION(prepare) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    LUA->CheckType(2, GarrysMod::Lua::Type::String);
    unsigned int outLen = 0;
    const char *queryStr = LUA->GetString(2, &outLen);
    auto query = PreparedQuery::create(database->m_database, std::string(queryStr, outLen));

    LUA->Push(1);
    int databaseRef = LuaReferenceCreate(LUA);

    auto luaQuery = new LuaPreparedQuery(query, databaseRef);

    pushLuaObjectTable(LUA, luaQuery, LuaObject::TYPE_PREPARED_QUERY);
    return 1;
}

MYSQLOO_LUA_FUNCTION(createTransaction) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    auto transaction = Transaction::create(database->m_database);

    LUA->Push(1);
    int databaseRef = LuaReferenceCreate(LUA);

    auto luaTransaction = new LuaTransaction(transaction, databaseRef);

    pushLuaObjectTable(LUA, luaTransaction, LuaObject::TYPE_TRANSACTION);
    return 1;
}

MYSQLOO_LUA_FUNCTION(connect) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    if (database->m_tableReference == 0) {
        LUA->Push(1);
        database->m_tableReference = LuaReferenceCreate(LUA);
    }
    database->m_database->connect();
    return 0;
}

MYSQLOO_LUA_FUNCTION(escape) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    unsigned int nQueryLength;
    const char *sQuery = LUA->GetString(2, &nQueryLength);
    auto escaped = database->m_database->escape(std::string(sQuery, nQueryLength));
    LUA->PushString(escaped.c_str(), (unsigned int) escaped.size());
    return 1;
}

MYSQLOO_LUA_FUNCTION(setCharacterSet) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    LUA->CheckType(2, GarrysMod::Lua::Type::String);
    const char *charset = LUA->GetString(2);
    bool success = database->m_database->setCharacterSet(charset);
    LUA->PushBool(success);
    LUA->PushString("");
    return 2;
}

MYSQLOO_LUA_FUNCTION(setSSLSettings) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    SSLSettings sslSettings;
    if (LUA->IsType(2, GarrysMod::Lua::Type::String)) {
        sslSettings.key = LUA->GetString(2);
    }
    if (LUA->IsType(3, GarrysMod::Lua::Type::String)) {
        sslSettings.cert = LUA->GetString(3);
    }
    if (LUA->IsType(4, GarrysMod::Lua::Type::String)) {
        sslSettings.ca = LUA->GetString(4);
    }
    if (LUA->IsType(5, GarrysMod::Lua::Type::String)) {
        sslSettings.capath = LUA->GetString(5);
    }
    if (LUA->IsType(6, GarrysMod::Lua::Type::String)) {
        sslSettings.cipher = LUA->GetString(6);
    }
    database->m_database->setSSLSettings(sslSettings);
    return 0;
}

MYSQLOO_LUA_FUNCTION(setReadTimeout) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    unsigned int timeout = (int) LUA->GetNumber(2);
    if (timeout == 0) {
        LUA->ThrowError("Timeout must be at least 1");
    }
    database->m_database->setReadTimeout(timeout);
    return 0;
}

MYSQLOO_LUA_FUNCTION(setWriteTimeout) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    unsigned int timeout = (int) LUA->GetNumber(2);
    if (timeout == 0) {
        LUA->ThrowError("Timeout must be at least 1");
    }
    database->m_database->setWriteTimeout(timeout);
    return 0;
}

MYSQLOO_LUA_FUNCTION(setConnectTimeout) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    unsigned int timeout = (int) LUA->GetNumber(2);
    if (timeout == 0) {
        LUA->ThrowError("Timeout must be at least 1");
    }
    database->m_database->setConnectTimeout(timeout);
    return 0;
}

MYSQLOO_LUA_FUNCTION(disconnect) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    bool wait = false;
    if (LUA->IsType(2, GarrysMod::Lua::Type::Bool)) {
        wait = LUA->GetBool(2);
    }
    database->m_database->disconnect(wait);
    if (wait) {
        database->think(LUA); //To set callback data, run callbacks
    }
    return 0;
}

MYSQLOO_LUA_FUNCTION(status) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    LUA->PushNumber(database->m_database->status());
    return 1;
}

MYSQLOO_LUA_FUNCTION(serverVersion) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    LUA->PushNumber(database->m_database->serverVersion());
    return 1;
}

MYSQLOO_LUA_FUNCTION(serverInfo) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    LUA->PushString(database->m_database->serverInfo().c_str());
    return 1;
}

MYSQLOO_LUA_FUNCTION(hostInfo) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    LUA->PushString(database->m_database->hostInfo().c_str());
    return 1;
}

MYSQLOO_LUA_FUNCTION(setAutoReconnect) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    LUA->CheckType(2, GarrysMod::Lua::Type::Bool);
    database->m_database->setShouldAutoReconnect(LUA->GetBool(2));
    return 0;
}

MYSQLOO_LUA_FUNCTION(setMultiStatements) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    LUA->CheckType(2, GarrysMod::Lua::Type::Bool);
    database->m_database->setMultiStatements(LUA->GetBool(2));
    return 0;
}

MYSQLOO_LUA_FUNCTION(setCachePreparedStatements) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    LUA->CheckType(2, GarrysMod::Lua::Type::Bool);
    database->m_database->setCachePreparedStatements(LUA->GetBool(2));
    return 0;
}

MYSQLOO_LUA_FUNCTION(abortAllQueries) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    auto abortedQueries = database->m_database->abortAllQueries();
    for (const auto &pair: abortedQueries) {
        LuaIQuery::runAbortedCallback(LUA, pair.second);
        LuaIQuery::finishQueryData(LUA, pair.first, pair.second);
    }
    LUA->PushNumber((double) abortedQueries.size());
    return 1;
}

MYSQLOO_LUA_FUNCTION(queueSize) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    LUA->PushNumber((double) database->m_database->queueSize());
    return 1;
}

MYSQLOO_LUA_FUNCTION(ping) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    LUA->PushBool(database->m_database->ping());
    return 1;
}

MYSQLOO_LUA_FUNCTION(wait) {
    auto database = LuaObject::getLuaObject<LuaDatabase>(LUA);
    database->m_database->wait();
    database->think(LUA); //To set callback data, run callbacks
    return 0;
}

void LuaDatabase::createMetaTable(ILuaBase *LUA) {
    LuaObject::TYPE_DATABASE = LUA->CreateMetaTable("MySQLOO Database");
    LuaObject::addMetaTableFunctions(LUA);

    LUA->PushCFunction(connect);
    LUA->SetField(-2, "connect");

    LUA->PushCFunction(escape);
    LUA->SetField(-2, "escape");

    LUA->PushCFunction(setCharacterSet);
    LUA->SetField(-2, "setCharacterSet");

    LUA->PushCFunction(setSSLSettings);
    LUA->SetField(-2, "setSSLSettings");

    LUA->PushCFunction(setReadTimeout);
    LUA->SetField(-2, "setReadTimeout");

    LUA->PushCFunction(setWriteTimeout);
    LUA->SetField(-2, "setWriteTimeout");

    LUA->PushCFunction(setConnectTimeout);
    LUA->SetField(-2, "setConnectTimeout");

    LUA->PushCFunction(disconnect);
    LUA->SetField(-2, "disconnect");

    LUA->PushCFunction(status);
    LUA->SetField(-2, "status");

    LUA->PushCFunction(serverVersion);
    LUA->SetField(-2, "serverVersion");

    LUA->PushCFunction(serverInfo);
    LUA->SetField(-2, "serverInfo");

    LUA->PushCFunction(hostInfo);
    LUA->SetField(-2, "hostInfo");

    LUA->PushCFunction(setAutoReconnect);
    LUA->SetField(-2, "setAutoReconnect");

    LUA->PushCFunction(setMultiStatements);
    LUA->SetField(-2, "setMultiStatements");

    LUA->PushCFunction(setCachePreparedStatements);
    LUA->SetField(-2, "setCachePreparedStatements");

    LUA->PushCFunction(abortAllQueries);
    LUA->SetField(-2, "abortAllQueries");

    LUA->PushCFunction(queueSize);
    LUA->SetField(-2, "queueSize");

    LUA->PushCFunction(ping);
    LUA->SetField(-2, "ping");

    LUA->PushCFunction(wait);
    LUA->SetField(-2, "wait");

    LUA->PushCFunction(query);
    LUA->SetField(-2, "query");

    LUA->PushCFunction(prepare);
    LUA->SetField(-2, "prepare");

    LUA->PushCFunction(createTransaction);
    LUA->SetField(-2, "createTransaction");

    LUA->Pop();
}

void LuaDatabase::think(ILuaBase *LUA) {
    //Connection callbacks
    auto database = this->m_database;
    if (database->isConnectionDone() && !this->m_dbCallbackRan && this->m_tableReference != 0) {
        this->m_dbCallbackRan = true;
        LUA->ReferencePush(this->m_tableReference);
        if (database->connectionSuccessful()) {
            LUA->GetField(-1, "onConnected");
            if (LUA->GetType(-1) == GarrysMod::Lua::Type::Function) {
                LUA->ReferencePush(this->m_tableReference);
                pcallWithErrorReporter(LUA, 1);
            }
            LUA->Pop(); //Callback function
        } else {
            LUA->GetField(-1, "onConnectionFailed");
            if (LUA->GetType(-1) == GarrysMod::Lua::Type::Function) {
                LUA->ReferencePush(this->m_tableReference);
                auto error = database->connectionError();
                LUA->PushString(error.c_str());
                pcallWithErrorReporter(LUA, 2);
            }
            LUA->Pop(); //Callback function
        }

        LuaReferenceFree(LUA, this->m_tableReference);
        this->m_tableReference = 0;
    }

    //Run callbacks of finished queries
    auto finishedQueries = database->takeFinishedQueries();
    for (auto &pair: finishedQueries) {
        LuaQuery::runCallback(LUA, pair.first, pair.second);
    }
}

void LuaDatabase::onDestroyedByLua(ILuaBase *LUA) {
    m_database->disconnect(true); //Wait for any outstanding queries to finish.
    //If this is called, LUA is either reloading or no queries exist in the query queue of the database, clear it
    //This needs to be cleared to avoid the queries leaking
    m_database->takeFinishedQueries();
    m_database->abortAllQueries();
}

/** Creates a weak table for all currently used databases.
  * And stores it in the table at the top of the stack at key "__weakDatabases"
  *
  * Expects the mysqloo table to be at the top of the stack.
  *
  * See runAllThinkHooks for why this is used.
  */
void LuaDatabase::createWeakTable(ILuaBase *LUA) {
    //Weak metatable
    LUA->CreateTable();
    LUA->PushString("k");
    LUA->SetField(-2, "__mode");

    //Weak table
    LUA->CreateTable();
    LUA->Push(-2); //Metatable
    LUA->SetMetaTable(-2);

    LUA->SetField(-3, "__weakDatabases");
    LUA->Pop(); //Metatable
}

/**
 * Runs the think hook for every database instance that is currently alive.
 * Expects the mysqloo table to be at the top of the stack.
 *
 * The idea of this function is to store a weak reference to each database's lua table and then
 * call the think function of each instance that is still alive.
 * If the table of a database instance is still alive, then so is the UserData object (which is stored in the table).
 *
 * This method eliminates the need of storing a global list of all database instances that are currently alive.
 */
void LuaDatabase::runAllThinkHooks(ILuaBase *LUA) {
    LUA->GetField(-1, "__weakDatabases");
    if (!LUA->IsType(-1, GarrysMod::Lua::Type::Table)) {
        LUA->Pop(); //Nil
        return;
    }
    //We iterate all (alive) entries in the weak table and create references to them.
    //This essentially creates a copy of the weak table that only contains strong references.
    //We need the copy so that no elements of the weak table are collected while think hooks of the databases
    //are executed, which might create new database instances, thus invalidating the iterator.
    std::vector<int> databaseReferences;
    LUA->PushNil(); //First key
    while (LUA->Next(-2) != 0) {
        //The key is the table of the database
        LUA->Push(-2); //The key, i.e. the database table
        databaseReferences.push_back(LuaReferenceCreate(LUA));

        LUA->Pop(); //The value, keep key on stack for next()
    }
    LUA->Pop(); //__weakDatabases

    //Call think function of each alive database instance
    for (auto &ref: databaseReferences) {
        LUA->ReferencePush(ref);
        LuaReferenceFree(LUA, ref); //We can immediately free this, the variable on the stack keeps it alive.
        auto database = LuaObject::getLuaObject<LuaDatabase>(LUA, -1);
        database->think(LUA);
        LUA->Pop(); //database
    }
}
