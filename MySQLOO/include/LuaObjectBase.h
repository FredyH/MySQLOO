#ifndef LUA_OBJECT_BASE_
#define LUA_OBJECT_BASE_
#include "GarrysMod/Lua/Interface.h"
#include "GarrysMod/Lua/Types.h"
#include <vector>
#include <map>
#include <atomic>
#include <deque>
#include <memory>
#include <map>
#include <algorithm>
#include <string>
enum {
	TYPE_DATABASE = 1,
	TYPE_QUERY = 2
};

class LuaObjectBase : public std::enable_shared_from_this<LuaObjectBase> {
public:
	LuaObjectBase(lua_State* state, bool shouldthink, unsigned char type);
	LuaObjectBase(lua_State* state, unsigned char type);
	virtual ~LuaObjectBase();
	virtual void think(lua_State* state) = 0;
	void registerFunction(lua_State* state, std::string name, GarrysMod::Lua::CFunc func);
	static int gcDeleteWrapper(lua_State* state);
	static int toStringWrapper(lua_State* state);
	static int createMetatables(lua_State* state);
	static int doThink(lua_State* state);
	static LuaObjectBase* unpackSelf(lua_State* state, int type = -1, bool shouldReference = false);
	static LuaObjectBase* unpackLuaObject(lua_State* state, int index, int type, bool shouldReference);
	int pushTableReference(lua_State* state);
	bool hasCallback(lua_State* state, const char* functionName);
	int getCallbackReference(lua_State* state, const char* functionName);
	void runFunction(lua_State* state, int funcRef, const char* sig = 0, ...);
	void runCallback(lua_State* state, const char* functionName, const char* sig = 0, ...);
	static std::deque<std::shared_ptr<LuaObjectBase>> luaObjects;
	static std::deque<std::shared_ptr<LuaObjectBase>> luaThinkObjects;
	static std::deque<std::shared_ptr<LuaObjectBase>> luaRemovalObjects;
	virtual void onDestroyed(lua_State* state) {};
	std::shared_ptr<LuaObjectBase> getSharedPointerInstance();
	void unreference(lua_State* state);
protected:
	void runFunctionVarList(lua_State* state, int funcRef, const char* sig, va_list list);
	bool scheduledForRemoval = false;
	bool shouldthink = false;
	int m_tableReference = 0;
	int m_userdataReference = 0;
	bool canbedestroyed = true;
	const char* classname = "LuaObject";
	unsigned char type = 255;
	static void referenceTable(lua_State* state, LuaObjectBase* object, int index);
private:
	std::map<std::string, GarrysMod::Lua::CFunc> m_callbackFunctions;
	static int tableMetaTable;
	static int userdataMetaTable;
};
#endif
