#include "GarrysMod/Lua/Interface.h"
#include "../mysql/Database.h"
#include <iostream>
#include <fstream>
#include "LuaObject.h"
#include "LuaDatabase.h"
#include "LuaTransaction.h"
#include "LuaQuery.h"
#include "LuaPreparedQuery.h"

#define MYSQLOO_VERSION "9"
#define MYSQLOO_MINOR_VERSION "7"

// Variable to hold the reference to the version check ConVar object
static int versionCheckConVar = 0;

GMOD_MODULE_CLOSE() {
    // Free the version check ConVar object reference
    if (versionCheckConVar != 0) {
        LuaReferenceFree(LUA, versionCheckConVar);
        versionCheckConVar = 0;
    }
    mysql_thread_end();
    mysql_library_end();

    return 0;
}

/* Returns the amount of currently allocated objects.
 */
LUA_FUNCTION(objectCount) {
    LUA->PushNumber((double) (LuaObject::allocationCount - LuaObject::deallocationCount));
    return 1;
}

static void runInTimer(GarrysMod::Lua::ILuaBase *LUA, double delay, GarrysMod::Lua::CFunc func) {
    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->GetField(-1, "timer");
    //In case someone removes the timer library
    if (LUA->IsType(-1, GarrysMod::Lua::Type::Nil)) {
        LUA->Pop(2);
        return;
    }
    LUA->GetField(-1, "Simple");
    LUA->PushNumber(delay);
    LUA->PushCFunction(func);
    LUA->Call(2, 0);
    LUA->Pop(2);
}

static void printMessage(GarrysMod::Lua::ILuaBase *LUA, const char *str, int r, int g, int b) {
    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->GetField(-1, "Color");
    LUA->PushNumber(r);
    LUA->PushNumber(g);
    LUA->PushNumber(b);
    LUA->Call(3, 1);
    int ref = LuaReferenceCreate(LUA);
    LUA->GetField(-1, "MsgC");
    LUA->ReferencePush(ref);
    LUA->PushString(str);
    LUA->Call(2, 0);
    LUA->Pop();
    LuaReferenceFree(LUA, ref);
}

static int printOutdatedVersion(lua_State *state) {
    GarrysMod::Lua::ILuaBase *LUA = state->luabase;
    LUA->SetState(state);
    printMessage(LUA, "Your server is using an outdated mysqloo9 version\n", 255, 0, 0);
    printMessage(LUA, "Download the latest version from here:\n", 255, 0, 0);
    printMessage(LUA, "https://github.com/FredyH/MySQLOO/releases\n", 86, 156, 214);
    runInTimer(LUA, 3600, printOutdatedVersion);
    return 0;
}

static int fetchSuccessful(lua_State *state) {
    GarrysMod::Lua::ILuaBase *LUA = state->luabase;
    LUA->SetState(state);
    std::string version = LUA->GetString(1);
    //version.size() < 3 so that the 404 response gets ignored
    if (version != MYSQLOO_MINOR_VERSION && version.size() <= 3) {
        printOutdatedVersion(state);
    } else {
        printMessage(LUA, "Your server is using the latest mysqloo9 version\n", 0, 255, 0);
    }
    return 0;
}

static int fetchFailed(lua_State *state) {
    GarrysMod::Lua::ILuaBase *LUA = state->luabase;
    LUA->SetState(state);
    printMessage(LUA, "Failed to retrieve latest version of mysqloo9\n", 255, 0, 0);
    return 0;
}

static int doVersionCheck(lua_State *state) {
    GarrysMod::Lua::ILuaBase *LUA = state->luabase;
    LUA->SetState(state);

    // Check if the reference to the ConVar object is set
    if (versionCheckConVar != 0) {
        // Retrieve the value of the ConVar
        LUA->ReferencePush(versionCheckConVar); // Push the ConVar object
        LUA->GetField(-1, "GetInt"); // Push the name of the function
        LUA->ReferencePush(versionCheckConVar); // Push the ConVar object as the first self argument
        LUA->Call(1, 1); // Call with 1 argument and 1 return
        int versionCheckEnabled = (int) LUA->GetNumber(-1); // Retrieve the returned value

        // Check if the version check convar is set to 1
        if (versionCheckEnabled == 1) {
            // Execute the HTTP request
            LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
            LUA->GetField(-1, "http");
            LUA->GetField(-1, "Fetch");
            LUA->PushString("https://raw.githubusercontent.com/FredyH/MySQLOO/master/minorversion.txt");
            LUA->PushCFunction(fetchSuccessful);
            LUA->PushCFunction(fetchFailed);
            LUA->PCall(3, 0, 0);
            LUA->Pop(2);
        }
    }

    return 0;
}

LUA_FUNCTION(allocationCount) {
    LUA->PushNumber(LuaObject::allocationCount);
    return 1;
}

LUA_FUNCTION(deallocationCount) {
    LUA->PushNumber(LuaObject::deallocationCount);
    return 1;
}

LUA_FUNCTION(referenceCreatedCount) {
    LUA->PushNumber((double) LuaObject::referenceCreatedCount);
    return 1;
}

LUA_FUNCTION(referenceFreedCount) {
    LUA->PushNumber((double) LuaObject::referenceFreedCount);
    return 1;
}

LUA_FUNCTION(mysqlooThink) {
    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->GetField(-1, "mysqloo");
    if (LUA->IsType(-1, GarrysMod::Lua::Type::Nil)) {
        LUA->Pop(2); //nil, Global
        return 0;
    }
    LuaDatabase::runAllThinkHooks(LUA);
    LUA->Pop(2); //nil, Global
    return 0;
}

GMOD_MODULE_OPEN() {
    if (mysql_library_init(0, nullptr, nullptr)) {
        LUA->ThrowError("Could not initialize mysql library.");
    }

    //Creating MetaTables
    LuaObject::createUserDataMetaTable(LUA);
    LuaDatabase::createMetaTable(LUA);
    LuaQuery::createMetaTable(LUA);
    LuaPreparedQuery::createMetaTable(LUA);
    LuaTransaction::createMetaTable(LUA);

    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->GetField(-1, "hook");
    LUA->GetField(-1, "Add");
    LUA->PushString("Think");
    LUA->PushString("__MySQLOOThinkHook");
    LUA->PushCFunction(mysqlooThink);
    LUA->Call(3, 0);
    LUA->Pop();
    LUA->Pop();
    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->CreateTable(); //mysqloo

    LUA->PushString(MYSQLOO_VERSION);
    LUA->SetField(-2, "VERSION");
    LUA->PushString(MYSQLOO_MINOR_VERSION);
    LUA->SetField(-2, "MINOR_VERSION");

    LUA->PushNumber(DATABASE_CONNECTED);
    LUA->SetField(-2, "DATABASE_CONNECTED");
    LUA->PushNumber(DATABASE_CONNECTING);
    LUA->SetField(-2, "DATABASE_CONNECTING");
    LUA->PushNumber(DATABASE_NOT_CONNECTED);
    LUA->SetField(-2, "DATABASE_NOT_CONNECTED");
    LUA->PushNumber(DATABASE_CONNECTION_FAILED);
    LUA->SetField(-2, "DATABASE_CONNECTION_FAILED");

    LUA->PushNumber(QUERY_NOT_RUNNING);
    LUA->SetField(-2, "QUERY_NOT_RUNNING");
    LUA->PushNumber(QUERY_RUNNING);
    LUA->SetField(-2, "QUERY_RUNNING");
    LUA->PushNumber(QUERY_COMPLETE);
    LUA->SetField(-2, "QUERY_COMPLETE");
    LUA->PushNumber(QUERY_ABORTED);
    LUA->SetField(-2, "QUERY_ABORTED");
    LUA->PushNumber(QUERY_WAITING);
    LUA->SetField(-2, "QUERY_WAITING");

    LUA->PushNumber(OPTION_NUMERIC_FIELDS);
    LUA->SetField(-2, "OPTION_NUMERIC_FIELDS");
    LUA->PushNumber(OPTION_INTERPRET_DATA);
    LUA->SetField(-2, "OPTION_INTERPRET_DATA"); //Not used anymore
    LUA->PushNumber(OPTION_NAMED_FIELDS);
    LUA->SetField(-2, "OPTION_NAMED_FIELDS"); //Not used anymore
    LUA->PushNumber(OPTION_CACHE);
    LUA->SetField(-2, "OPTION_CACHE"); //Not used anymore

    LUA->PushCFunction(LuaDatabase::create);
    LUA->SetField(-2, "connect");

    //Debug/testing functions
    LUA->PushCFunction(objectCount);
    LUA->SetField(-2, "objectCount");
    LUA->PushCFunction(allocationCount);
    LUA->SetField(-2, "allocationCount");
    LUA->PushCFunction(deallocationCount);
    LUA->SetField(-2, "deallocationCount");
    LUA->PushCFunction(referenceFreedCount);
    LUA->SetField(-2, "referenceFreedCount");
    LUA->PushCFunction(referenceCreatedCount);
    LUA->SetField(-2, "referenceCreatedCount");


    LuaDatabase::createWeakTable(LUA);

    LUA->SetField(-2, "mysqloo");
    LUA->Pop();

    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB); // Push the global table
    // Create the version check ConVar
    LUA->GetField(-1, "CreateConVar");
    LUA->PushString("sv_mysqloo_versioncheck"); // Name
    LUA->PushString("1"); // Default value
    LUA->PushNumber(128); // FCVAR flags
    LUA->PushString("Enable or disable the MySQLOO update checker."); // Help text
    LUA->PushNumber(0); // Min value
    LUA->PushNumber(1); // Max value
    LUA->Call(6, 1); // Call with 6 arguments and 1 result
    versionCheckConVar = LuaReferenceCreate(LUA); // Store the created ConVar object as a global variable
    LUA->Pop(); // Pop the global table

    runInTimer(LUA, 5, doVersionCheck);

    return 1;
}
