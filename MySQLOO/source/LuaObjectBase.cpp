#include "LuaObjectBase.h"
#include "Logger.h"
#include <stdarg.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>

std::deque<std::shared_ptr<LuaObjectBase>> LuaObjectBase::luaObjects = std::deque<std::shared_ptr<LuaObjectBase>>();
std::deque<std::shared_ptr<LuaObjectBase>> LuaObjectBase::luaThinkObjects = std::deque<std::shared_ptr<LuaObjectBase>>();
std::deque<std::shared_ptr<LuaObjectBase>> LuaObjectBase::luaRemovalObjects = std::deque<std::shared_ptr<LuaObjectBase>>();
int LuaObjectBase::tableMetaTable = 0;
int LuaObjectBase::userdataMetaTable = 0;


LuaObjectBase::LuaObjectBase(lua_State* state, bool shouldthink, unsigned char type) : type(type)
{
	LOG_CURRENT_FUNCTIONCALL
	classname = "LuaObject";
	this->shouldthink = shouldthink;
	std::shared_ptr<LuaObjectBase> ptr(this);
	if (shouldthink)
	{
		luaThinkObjects.push_back(ptr);
	}
	luaObjects.push_back(ptr);
}

LuaObjectBase::LuaObjectBase(lua_State* state, unsigned char type) : LuaObjectBase::LuaObjectBase(state, true, type)
{
}


//Important!!!!
//LuaObjectBase should never be deleted manually
//Let shared_ptrs handle it
LuaObjectBase::~LuaObjectBase()
{
}

//Makes C++ functions callable from lua
void LuaObjectBase::registerFunction(lua_State* state, std::string name, GarrysMod::Lua::CFunc func)
{
	LOG_CURRENT_FUNCTIONCALL
	this->m_callbackFunctions[name] = func;
}

//Returns the shared_ptr that exists for this instance
std::shared_ptr<LuaObjectBase> LuaObjectBase::getSharedPointerInstance()
{
	return shared_from_this();
}

//Gets the C++ object associated with a lua table that represents it in LUA
LuaObjectBase* LuaObjectBase::unpackSelf(lua_State* state, int type, bool shouldReference)
{
	LOG_CURRENT_FUNCTIONCALL
	LUA->CheckType(1, GarrysMod::Lua::Type::TABLE);
	LUA->GetField(1, "___lua_userdata_object");
	GarrysMod::Lua::UserData* ud = (GarrysMod::Lua::UserData*) LUA->GetUserdata(-1);
	if (ud->type != type && type != -1)
	{
		std::ostringstream oss;
		oss << "Wrong type, expected " << type << " got " << ((int)ud->type);
		LUA->ThrowError(oss.str().c_str());
	}
	LuaObjectBase* object = (LuaObjectBase*)ud->data;
	if (shouldReference && object->m_userdataReference == 0 && object->m_tableReference == 0)
	{
		object->m_userdataReference = LUA->ReferenceCreate();
		LUA->Push(1); //Pushes table that needs to be referenced
		object->m_tableReference = LUA->ReferenceCreate();
	}
	else
	{
		LUA->Pop();
	}
	return object;
}

//Pushes the table reference of a C++ object that represents it in LUA
int LuaObjectBase::pushTableReference(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	if (m_tableReference != 0)
	{
		LUA->ReferencePush(m_tableReference);
		return 1;
	}
	GarrysMod::Lua::UserData* ud = (GarrysMod::Lua::UserData*) LUA->NewUserdata(sizeof(GarrysMod::Lua::UserData));
	int userdatareference = LUA->ReferenceCreate();
	LUA->ReferencePush(userdatareference);
	LUA->ReferencePush(userdataMetaTable);
	LUA->SetMetaTable(-2);
	LUA->Pop();
	ud->data = this;
	ud->type = this->type;
	LUA->CreateTable();
	LUA->ReferencePush(userdatareference);
	LUA->SetField(-2, "___lua_userdata_object");
	for (auto it = m_callbackFunctions.begin(); it != m_callbackFunctions.end(); it++)
	{
		LUA->PushCFunction(it->second);
		LUA->SetField(-2, it->first.c_str());
	}
	LUA->ReferencePush(tableMetaTable);
	LUA->SetMetaTable(-2);
	LUA->ReferenceFree(userdatareference);
	return 1;
}

//Unreferences the table that represents this C++ object in lua, so that it can be gc'ed
void LuaObjectBase::unreference(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	if (m_tableReference != 0)
	{
		LUA->ReferenceFree(m_tableReference);
		m_tableReference = 0;
	}
	if (m_userdataReference != 0)
	{
		LUA->ReferenceFree(m_userdataReference);
		m_userdataReference = 0;
	}
}

//Checks whether or not a callback exists
bool LuaObjectBase::hasCallback(lua_State* state, const char* functionName)
{
	LOG_CURRENT_FUNCTIONCALL
	if (this->m_tableReference == 0) return false;
	LUA->ReferencePush(this->m_tableReference);
	LUA->GetField(-1, functionName);
	bool hasCallback = LUA->GetType(-1) == GarrysMod::Lua::Type::FUNCTION;
	LUA->Pop(2);
	return hasCallback;
}

//Runs callbacks associated with the lua object
void LuaObjectBase::runCallback(lua_State* state, const char* functionName, const char* sig, ...)
{
	LOG_CURRENT_FUNCTIONCALL
	if (this->m_tableReference == 0) return;
	LUA->ReferencePush(this->m_tableReference);
	LUA->GetField(-1, functionName);
	if (LUA->GetType(-1) != GarrysMod::Lua::Type::FUNCTION)
	{
		LUA->Pop(2);
		return;
	}
	pushTableReference(state);
	int numArguments = 1;
	if (sig)
	{
		va_list arguments;
		va_start(arguments, sig);
		for (unsigned int i = 0; i < std::strlen(sig); i++)
		{
			char option = sig[i];
			if (option == 'i')
			{
				int value = va_arg(arguments, int);
				LUA->PushNumber(value);
				numArguments++;
			}
			else if (option == 'f')
			{
				float value = static_cast<float>(va_arg(arguments, double));
				LUA->PushNumber(value);
				numArguments++;
			}
			else if (option == 'b')
			{
				bool value = va_arg(arguments, int) != 0;
				LUA->PushBool(value);
				numArguments++;
			}
			else if (option == 's')
			{
				char* value = va_arg(arguments, char*);
				LUA->PushString(value);
				numArguments++;
			}
			else if (option == 'o')
			{
				int value = va_arg(arguments, int);
				LUA->ReferencePush(value);
				numArguments++;
			}
			else if (option == 'r')
			{
				int reference = va_arg(arguments, int);
				LUA->ReferencePush(reference);
				numArguments++;
			}
			else if (option == 'F')
			{
				GarrysMod::Lua::CFunc value = va_arg(arguments, GarrysMod::Lua::CFunc);
				LUA->PushCFunction(value);
				numArguments++;
			}
		}
		va_end(arguments);
	}
	LUA->PCall(numArguments, 0, -2 - numArguments);
	LUA->Pop(1);
}

//Called every tick, checks if the object can be destroyed
int LuaObjectBase::doThink(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	//Think objects need to be copied because a think call could modify the original thinkObject queue
	//which leads to it invalidating the iterator and thus undefined behaviour
	std::deque<std::shared_ptr<LuaObjectBase>> thinkObjectsCopy = luaThinkObjects;
	for (auto it = thinkObjectsCopy.begin(); it != thinkObjectsCopy.end(); ++it) {
		(*it)->think(state);
	}
	if (luaRemovalObjects.size() > 0)
	{
		for (auto it = luaRemovalObjects.begin(); it != luaRemovalObjects.end(); )
		{
			LuaObjectBase* obj = (*it).get();
			if (obj->canbedestroyed)
			{
				obj->onDestroyed(state);
				it = luaRemovalObjects.erase(it);
				auto objectIt = std::find_if(luaObjects.begin(), luaObjects.end(), [&](std::shared_ptr<LuaObjectBase> const& p) {
					return p.get() == obj;
				});
				if (objectIt != luaObjects.end())
				{
					luaObjects.erase(objectIt);
				}
				auto thinkObjectIt = std::find_if(luaThinkObjects.begin(), luaThinkObjects.end(), [&](std::shared_ptr<LuaObjectBase> const& p) {
					return p.get() == obj;
				});
				if (thinkObjectIt != luaThinkObjects.end())
				{
					luaThinkObjects.erase(thinkObjectIt);
				}
			}
			else
			{
				++it;
			}
		}
	}
	return 0;
}

//Called when the LUA table representing this C++ object has been gc'ed
//Deletes the associated C++ object
int LuaObjectBase::gcDeleteWrapper(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	GarrysMod::Lua::UserData* obj = (GarrysMod::Lua::UserData*) LUA->GetUserdata(1);
	if (!obj || (obj->type != TYPE_DATABASE && obj->type != TYPE_QUERY))
		return 0;
	LuaObjectBase* object = (LuaObjectBase*)obj->data;
	if (!object->scheduledForRemoval)
	{
		if (object->m_userdataReference != 0)
		{
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
int LuaObjectBase::toStringWrapper(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	LuaObjectBase* object = unpackSelf(state);
	std::stringstream ss;
	ss << object->classname << " " << object;
	LUA->PushString(ss.str().c_str());
	return 1;
}

//Creates metatables used for the LUA representation of the C++ table
int LuaObjectBase::createMetatables(lua_State* state)
{
	LOG_CURRENT_FUNCTIONCALL
	LUA->CreateTable();

	LUA->PushCFunction(LuaObjectBase::gcDeleteWrapper);
	LUA->SetField(-2, "__gc");
	userdataMetaTable = LUA->ReferenceCreate();

	LUA->CreateTable();
	LUA->PushCFunction(LuaObjectBase::toStringWrapper);
	LUA->SetField(-2, "__tostring");
	tableMetaTable = LUA->ReferenceCreate();
	return 0;
}