#include "LuaObjectBase.h"
#include <stdarg.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>

int TYPE_DATABASE = -1;
int TYPE_QUERY = -1;

std::deque<std::shared_ptr<LuaObjectBase>> LuaObjectBase::luaObjects = std::deque<std::shared_ptr<LuaObjectBase>>();
std::deque<std::shared_ptr<LuaObjectBase>> LuaObjectBase::luaThinkObjects = std::deque<std::shared_ptr<LuaObjectBase>>();
std::deque<std::shared_ptr<LuaObjectBase>> LuaObjectBase::luaRemovalObjects = std::deque<std::shared_ptr<LuaObjectBase>>();
int LuaObjectBase::tableMetaTable = 0;


LuaObjectBase::LuaObjectBase(GarrysMod::Lua::ILuaBase* LUA, bool shouldthink, unsigned char type) : type(type) {
	classname = "LuaObject";
	this->shouldthink = shouldthink;
	std::shared_ptr<LuaObjectBase> ptr(this);
	if (shouldthink) {
		luaThinkObjects.push_back(ptr);
	}
	luaObjects.push_back(ptr);
}

LuaObjectBase::LuaObjectBase(GarrysMod::Lua::ILuaBase* LUA, unsigned char type) : LuaObjectBase::LuaObjectBase(LUA, true, type) {}


//Important!!!!
//LuaObjectBase should never be deleted manually
//Let shared_ptrs handle it
LuaObjectBase::~LuaObjectBase() {}

//Makes C++ functions callable from lua
void LuaObjectBase::registerFunction(GarrysMod::Lua::ILuaBase* LUA, std::string name, GarrysMod::Lua::CFunc func) {
	this->m_callbackFunctions[name] = func;
}

//Returns the shared_ptr that exists for this instance
std::shared_ptr<LuaObjectBase> LuaObjectBase::getSharedPointerInstance() {
	return shared_from_this();
}

//Gets the C++ object associated with a lua table that represents it in LUA
LuaObjectBase* LuaObjectBase::unpackSelf(GarrysMod::Lua::ILuaBase* LUA, int type, bool shouldReference) {
	return unpackLuaObject(LUA, 1, type, shouldReference);
}

//Gets the C++ object associated with a lua table that represents it in LUA
LuaObjectBase* LuaObjectBase::unpackLuaObject(GarrysMod::Lua::ILuaBase* LUA, int index, int type, bool shouldReference) {
	LUA->CheckType(index, GarrysMod::Lua::Type::TABLE);
	LUA->GetField(index, "___lua_userdata_object");
	int itype = LUA->GetType(-1);
	LuaObjectBase* object = NULL;
	if (itype == type || type == -1) {
		object = LUA->GetUserType<LuaObjectBase>(-1, itype);
	}
	if (object == NULL) {
		std::ostringstream oss;
		oss << "Wrong type, expected " << type << " got " << itype;
		LUA->ThrowError(oss.str().c_str());
	}
	if (shouldReference) {
		referenceTable(LUA, object, index);
	}
	LUA->Pop();
	return object;
}

void LuaObjectBase::referenceTable(GarrysMod::Lua::ILuaBase* LUA, LuaObjectBase* object, int index) {
	if (object->m_userdataReference != 0 || object->m_tableReference != 0) {
		LUA->ThrowError("Tried to reference lua object twice (Query started twice?)");
	}
	LUA->CheckType(index, GarrysMod::Lua::Type::TABLE);
	LUA->Push(index);
	object->m_tableReference = LUA->ReferenceCreate();
	LUA->Push(index);
	LUA->GetField(index, "___lua_userdata_object");
	object->m_userdataReference = LUA->ReferenceCreate();
}

//Pushes the table reference of a C++ object that represents it in LUA
int LuaObjectBase::pushTableReference(GarrysMod::Lua::ILuaBase* LUA) {
	if (m_tableReference != 0) {
		LUA->ReferencePush(m_tableReference);
		return 1;
	}

	LUA->PushUserType(this, type);

	LUA->PushMetaTable(type);
	LUA->SetMetaTable(-2);

	LUA->CreateTable();

	LUA->Push(-2);
	LUA->SetField(-2, "___lua_userdata_object");

	for (auto& callback : this->m_callbackFunctions) {
		LUA->PushCFunction(callback.second);
		LUA->SetField(-2, callback.first.c_str());
	}

	LUA->PushMetaTable(tableMetaTable);
	LUA->SetMetaTable(-2);

	LUA->Remove(-2);

	return 1;
}

//Unreferences the table that represents this C++ object in lua, so that it can be gc'ed
void LuaObjectBase::unreference(GarrysMod::Lua::ILuaBase* LUA) {
	if (m_tableReference != 0) {
		LUA->ReferenceFree(m_tableReference);
		m_tableReference = 0;
	}
	if (m_userdataReference != 0) {
		LUA->ReferenceFree(m_userdataReference);
		m_userdataReference = 0;
	}
}

//Checks whether or not a callback exists
bool LuaObjectBase::hasCallback(GarrysMod::Lua::ILuaBase* LUA, const char* functionName) {
	if (this->m_tableReference == 0) return false;
	LUA->ReferencePush(this->m_tableReference);
	LUA->GetField(-1, functionName);
	bool hasCallback = LUA->GetType(-1) == GarrysMod::Lua::Type::FUNCTION;
	LUA->Pop(2);
	return hasCallback;
}

int LuaObjectBase::getCallbackReference(GarrysMod::Lua::ILuaBase* LUA, const char* functionName) {
	if (this->m_tableReference == 0) return 0;
	LUA->ReferencePush(this->m_tableReference);
	LUA->GetField(-1, functionName);
	if (LUA->GetType(-1) != GarrysMod::Lua::Type::FUNCTION) {
		LUA->Pop(2);
		return 0;
	}
	//Hacky solution so there isn't too much stuff on the stack after this
	int ref = LUA->ReferenceCreate();
	return ref;
}

void LuaObjectBase::runFunction(GarrysMod::Lua::ILuaBase* LUA, int funcRef, const char* sig, ...) {
	if (funcRef == 0) return;
	va_list arguments;
	va_start(arguments, sig);
	runFunctionVarList(LUA, funcRef, sig, arguments);
	va_end(arguments);
}

static int buildErrorStack(lua_State *state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->GetField(-1, "debug");
	LUA->GetField(-1, "traceback");

	if (LUA->IsType(-1, GarrysMod::Lua::Type::FUNCTION)) {
		LUA->Push(1);
		LUA->Call(1, 1);
	}
	else {
		LUA->Pop(3); // global, traceback
	}


	return 1;
}

void LuaObjectBase::runFunctionVarList(GarrysMod::Lua::ILuaBase* LUA, int funcRef, const char* sig, va_list arguments) {
	if (funcRef == 0) return;
	if (this->m_tableReference == 0) return;
	
	LUA->PushCFunction(buildErrorStack);
	int errorReporterIndex = LUA->Top();

	LUA->ReferencePush(funcRef);
	pushTableReference(LUA);
	int numArguments = 1;
	if (sig) {
		for (unsigned int i = 0; i < std::strlen(sig); i++) {
			char option = sig[i];
			if (option == 'i') {
				int value = va_arg(arguments, int);
				LUA->PushNumber(value);
				numArguments++;
			} else if (option == 'f') {
				float value = static_cast<float>(va_arg(arguments, double));
				LUA->PushNumber(value);
				numArguments++;
			} else if (option == 'b') {
				bool value = va_arg(arguments, int) != 0;
				LUA->PushBool(value);
				numArguments++;
			} else if (option == 's') {
				char* value = va_arg(arguments, char*);
				LUA->PushString(value);
				numArguments++;
			} else if (option == 'o') {
				int value = va_arg(arguments, int);
				LUA->ReferencePush(value);
				numArguments++;
			} else if (option == 'r') {
				int reference = va_arg(arguments, int);
				LUA->ReferencePush(reference);
				numArguments++;
			} else if (option == 'F') {
				GarrysMod::Lua::CFunc value = va_arg(arguments, GarrysMod::Lua::CFunc);
				LUA->PushCFunction(value);
				numArguments++;
			}
		}
	}
	
	if (LUA->PCall(numArguments, 0, errorReporterIndex)) {
		LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		LUA->GetField(-1, "ErrorNoHalt");
		//In case someone removes ErrorNoHalt this doesn't break everything
		if (LUA->IsType(-1, GarrysMod::Lua::Type::FUNCTION)) {
			LUA->Push(-3); // error
			LUA->PushString("\n");
			LUA->Call(2, 0);
		}
		else
			LUA->Pop(1); // function

		LUA->Pop(2); // error, global
	}

	LUA->Pop(1); // error function
}

//Runs callbacks associated with the lua object
void LuaObjectBase::runCallback(GarrysMod::Lua::ILuaBase* LUA, const char* functionName, const char* sig, ...) {
	if (this->m_tableReference == 0) return;
	int funcRef = getCallbackReference(LUA, functionName);
	if (funcRef == 0) {
		LUA->ReferenceFree(funcRef);
		return;
	}
	va_list arguments;
	va_start(arguments, sig);
	runFunctionVarList(LUA, funcRef, sig, arguments);
	va_end(arguments);
	LUA->ReferenceFree(funcRef);
}

//Called every tick, checks if the object can be destroyed
int LuaObjectBase::doThink(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	//Think objects need to be copied because a think call could modify the original thinkObject queue
	//which leads to it invalidating the iterator and thus undefined behaviour
	std::deque<std::shared_ptr<LuaObjectBase>> thinkObjectsCopy = luaThinkObjects;
	for (auto& query : luaThinkObjects) {
		query->think(LUA);
	}
	if (luaRemovalObjects.size() > 0) {
		for (auto it = luaRemovalObjects.begin(); it != luaRemovalObjects.end(); ) {
			LuaObjectBase* obj = (*it).get();
			if (obj->canbedestroyed) {
				obj->onDestroyed(LUA);
				it = luaRemovalObjects.erase(it);
				auto objectIt = std::find_if(luaObjects.begin(), luaObjects.end(), [&](std::shared_ptr<LuaObjectBase> const& p) {
					return p.get() == obj;
				});
				if (objectIt != luaObjects.end()) {
					luaObjects.erase(objectIt);
				}
				auto thinkObjectIt = std::find_if(luaThinkObjects.begin(), luaThinkObjects.end(), [&](std::shared_ptr<LuaObjectBase> const& p) {
					return p.get() == obj;
				});
				if (thinkObjectIt != luaThinkObjects.end()) {
					luaThinkObjects.erase(thinkObjectIt);
				}
			} else {
				++it;
			}
		}
	}
	return 0;
}

//Called when the LUA table representing this C++ object has been gc'ed
//Deletes the associated C++ object
int LuaObjectBase::gcDeleteWrapper(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	int type = LUA->GetType(1);
	if (type != TYPE_DATABASE && type != TYPE_QUERY) return 0;
	LuaObjectBase* object = LUA->GetUserType<LuaObjectBase>(1, type);
	if (object == NULL) return 0;
	if (!object->scheduledForRemoval) {
		if (object->m_userdataReference != 0) {
			LUA->ReferenceFree(object->m_userdataReference);
			object->m_userdataReference = 0;
		}
		object->scheduledForRemoval = true;
		//This can't go wrong, lua objects are only ever deleted in the main thread,
		//which this function is being called by, so the object can't be deleted yet
		luaRemovalObjects.push_back(object->getSharedPointerInstance());
	}
	return 0;
}

//Prints the name of the object
int LuaObjectBase::toStringWrapper(lua_State* state) {
	GarrysMod::Lua::ILuaBase* LUA = state->luabase;
	LUA->SetState(state);
	LuaObjectBase* object = unpackSelf(LUA);
	std::stringstream ss;
	ss << object->classname << " " << object;
	LUA->PushString(ss.str().c_str());
	return 1;
}

//Creates metatables used for the LUA representation of the C++ table
int LuaObjectBase::createMetatables(GarrysMod::Lua::ILuaBase* LUA) {
	TYPE_DATABASE = LUA->CreateMetaTable("MySQLOO database");
	LUA->PushCFunction(LuaObjectBase::gcDeleteWrapper);
	LUA->SetField(-2, "__gc");

	TYPE_QUERY = LUA->CreateMetaTable("MySQLOO query");
	LUA->PushCFunction(LuaObjectBase::gcDeleteWrapper);
	LUA->SetField(-2, "__gc");

	tableMetaTable = LUA->CreateMetaTable("MySQLOO table");
	LUA->PushCFunction(LuaObjectBase::toStringWrapper);
	LUA->SetField(-2, "__tostring");

	return 0;
}