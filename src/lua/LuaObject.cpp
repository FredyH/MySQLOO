#include <algorithm>
#include "LuaObject.h"
#include "LuaDatabase.h"
#include <iostream>

int LuaObject::TYPE_USERDATA = 0;
int LuaObject::TYPE_DATABASE = 0;
int LuaObject::TYPE_QUERY = 0;
int LuaObject::TYPE_TRANSACTION = 0;
int LuaObject::TYPE_PREPARED_QUERY = 0;

std::atomic_long LuaObject::allocationCount= { 0 };
std::atomic_long LuaObject::deallocationCount = { 0 };

LUA_FUNCTION(luaObjectGc) {
    auto luaObject = LUA->GetUserType<LuaObject>(1, LuaObject::TYPE_USERDATA);
    luaObject->onDestroyedByLua(LUA);

    delete luaObject;

    //After this function this object will be deleted.
    //For the Database this might cause the database thread to join
    //But the database can only be destroyed if no queries for it exist, i.e. joining
    //should always work instantly, unless the server is changing maps, in which case we want it to wait.
    return 0;
}

void LuaObject::createUserDataMetaTable(GarrysMod::Lua::ILuaBase *LUA) {
    TYPE_USERDATA = LUA->CreateMetaTable("MySQLOO UserData");
    LUA->PushCFunction(luaObjectGc);
    LUA->SetField(-2, "__gc");
    LUA->Pop();
}

std::string LuaObject::toString() {
    std::stringstream ss;
    ss << m_className << " " << this;
    return ss.str();
}

LUA_FUNCTION(luaObjectToString) {
    LUA->GetUserType<LuaObject>(1, LuaObject::TYPE_USERDATA);
    auto luaObject = LuaObject::getLuaObject<LuaObject>(LUA);
    auto str = luaObject->toString();
    LUA->PushString(str.c_str());
    return 1;
}

void LuaObject::addMetaTableFunctions(GarrysMod::Lua::ILuaBase *LUA) {
    LUA->Push(-1);
    LUA->SetField(-2, "__index");

    LUA->PushCFunction(luaObjectToString);
    LUA->SetField(-2, "__tostring");
}

LUA_FUNCTION(errorReporter) {
    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->GetField(-1, "debug");
    LUA->GetField(-1, "traceback");

    if (LUA->IsType(-1, GarrysMod::Lua::Type::Function)) {
        LUA->Push(1);
        LUA->Call(1, 1); //The resulting stack trace will be returned
    } else {
        LUA->PushString("MySQLOO Error");
    }
    return 1;
}

/**
 * Similar to LUA->PCall but also uses an error reporter and prints the
 * error to the console using ErrorNoHalt (if it exists).
 * Consumes the function and all nargs arguments on the stack, does not return any values.
 */
void LuaObject::pcallWithErrorReporter(ILuaBase *LUA, int nargs) {
    LUA->PushCFunction(errorReporter);
    int errorHandlerIndex = LUA->Top() - nargs - 1;
    LUA->Insert(errorHandlerIndex);
    int pcallResult = LUA->PCall(nargs, 0, errorHandlerIndex);
    if (pcallResult == 2) { //LUA_ERRRUN, we now have a stack trace on the stack
        LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
        LUA->GetField(-1, "ErrorNoHalt");
        if (LUA->IsType(-1, GarrysMod::Lua::Type::Function)) {
            LUA->Push(-3); //Stack trace
            LUA->Call(1, 0);
            LUA->Pop(2); //Stack trace, global
        } else {
            LUA->Pop(3); //Stack trace, global, nil
        }
    }
    LUA->Pop(); //Error reporter
}

bool LuaObject::pushCallbackReference(ILuaBase *LUA, int functionReference, int tableReference,
                                      const std::string &callbackName, bool allowCallback) {
    //Push function reference
    if (functionReference != 0) {
        LUA->ReferencePush(functionReference);
        return true;
    } else if (allowCallback && tableReference != 0) {
        LUA->ReferencePush(tableReference);
        LUA->GetField(-1, callbackName.c_str());
        LUA->Remove(LUA->Top() - 2);
        if (LUA->IsType(-1, GarrysMod::Lua::Type::Function)) {
            return true;
        } else {
            LUA->Pop(1); //The field that is either nil or some other weird thing
            return false;
        }
    } else {
        return false;
    }
}


int LuaObject::getFunctionReference(ILuaBase *LUA, int stackPosition, const char* fieldName) {
    LUA->GetField(stackPosition, fieldName);
    int reference = 0;
    if (LUA->IsType(-1, GarrysMod::Lua::Type::Function)) {
        reference = LuaReferenceCreate(LUA);
    } else {
        LUA->Pop();
    }
    return reference;
}

uint64_t LuaObject::referenceCreatedCount = 0;
uint64_t LuaObject::referenceFreedCount = 0;

int LuaReferenceCreate(GarrysMod::Lua::ILuaBase *LUA) {
    LuaObject::referenceCreatedCount++;
    return LUA->ReferenceCreate();
}

void LuaReferenceFree(GarrysMod::Lua::ILuaBase *LUA, int ref) {
    LuaObject::referenceFreedCount++;
    LUA->ReferenceFree(ref);
}