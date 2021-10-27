#ifndef GARRYSMOD_LUA_INTERFACE_H
#define GARRYSMOD_LUA_INTERFACE_H

#include "LuaBase.h"

struct lua_State
{
#if defined( _WIN32 ) && !defined( _M_X64 )
    // Win32
    unsigned char _ignore_this_common_lua_header_[48 + 22];
#elif defined( _WIN32 ) && defined( _M_X64 )
    // Win64
    unsigned char _ignore_this_common_lua_header_[92 + 22];
#elif defined( __linux__ ) && !defined( __x86_64__ )
    // Linux32
    unsigned char _ignore_this_common_lua_header_[48 + 22];
#elif defined( __linux__ ) && defined( __x86_64__ )
    // Linux64
    unsigned char _ignore_this_common_lua_header_[92 + 22];
#elif defined ( __APPLE__ ) && !defined( __x86_64__ )
    // macOS32
    unsigned char _ignore_this_common_lua_header_[48 + 22];
#elif defined ( __APPLE__ ) && defined( __x86_64__ )
    // macOS64
    unsigned char _ignore_this_common_lua_header_[92 + 22];
#else
    #error agh
#endif

    GarrysMod::Lua::ILuaBase* luabase;
};

#ifndef GMOD
    #ifdef _WIN32
        #define DLL_EXPORT extern "C" __declspec( dllexport )
    #else
        #define DLL_EXPORT extern "C" __attribute__((visibility("default")))
    #endif

    #ifdef GMOD_ALLOW_DEPRECATED
        // Stop using this and use LUA_FUNCTION!
        #define LUA ( state->luabase )

        #define GMOD_MODULE_OPEN()  DLL_EXPORT int gmod13_open( lua_State* state )
        #define GMOD_MODULE_CLOSE() DLL_EXPORT int gmod13_close( lua_State* state )
    #else
        #define GMOD_MODULE_OPEN()                                 \
            int gmod13_open__Imp( GarrysMod::Lua::ILuaBase* LUA ); \
            DLL_EXPORT int gmod13_open( lua_State* L )             \
            {                                                      \
                return gmod13_open__Imp( L->luabase );             \
            }                                                      \
            int gmod13_open__Imp( GarrysMod::Lua::ILuaBase* LUA )

        #define GMOD_MODULE_CLOSE()                                 \
            int gmod13_close__Imp( GarrysMod::Lua::ILuaBase* LUA ); \
            DLL_EXPORT int gmod13_close( lua_State* L )             \
            {                                                       \
                return gmod13_close__Imp( L->luabase );             \
            }                                                       \
            int gmod13_close__Imp( GarrysMod::Lua::ILuaBase* LUA )

        #define LUA_FUNCTION( FUNC )                          \
            int FUNC##__Imp( GarrysMod::Lua::ILuaBase* LUA ); \
            int FUNC( lua_State* L )                          \
            {                                                 \
                GarrysMod::Lua::ILuaBase* LUA = L->luabase;   \
                LUA->SetState(L);                             \
                return FUNC##__Imp( LUA );                    \
            }                                                 \
            int FUNC##__Imp( GarrysMod::Lua::ILuaBase* LUA )
    #endif
#endif

#endif
