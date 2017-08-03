#ifndef GARRYSMOD_LUA_TYPES_H
#define GARRYSMOD_LUA_TYPES_H

#ifdef ENTITY
#undef ENTITY
#endif

#ifdef VECTOR
#undef VECTOR
#endif

namespace GarrysMod
{
    namespace Lua
    {
        namespace Type
        {
            enum
            {
#ifdef GMOD_ALLOW_DEPRECATED
                // Deprecated: Use NONE instead of INVALID
                INVALID = -1,
#endif

                // Lua Types
                NONE = -1,
                NIL,
                BOOL,
                LIGHTUSERDATA,
                NUMBER,
                STRING,
                TABLE,
                FUNCTION,
                USERDATA,
                THREAD,

                // GMod Types
                ENTITY,
                VECTOR,
                ANGLE,
                PHYSOBJ,
                SAVE,
                RESTORE,
                DAMAGEINFO,
                EFFECTDATA,
                MOVEDATA,
                RECIPIENTFILTER,
                USERCMD,
                SCRIPTEDVEHICLE,
                MATERIAL,
                PANEL,
                PARTICLE,
                PARTICLEEMITTER,
                TEXTURE,
                USERMSG,
                CONVAR,
                IMESH,
                MATRIX,
                SOUND,
                PIXELVISHANDLE,
                DLIGHT,
                VIDEO,
                FILE,
                LOCOMOTION,
                PATH,
                NAVAREA,
                SOUNDHANDLE,
                NAVLADDER,
                PARTICLESYSTEM,
                PROJECTEDTEXTURE,

                COUNT
            };

#if ( defined( GMOD ) || defined( GMOD_ALLOW_DEPRECATED ) )
            // You should use ILuaBase::GetTypeName instead of directly accessing this array
            static const char* Name[] =
            {
                "nil",
                "bool",
                "lightuserdata",
                "number",
                "string",
                "table",
                "function",
                "userdata",
                "thread",
                "entity",
                "vector",
                "angle",
                "physobj",
                "save",
                "restore",
                "damageinfo",
                "effectdata",
                "movedata",
                "recipientfilter",
                "usercmd",
                "vehicle",
                "material",
                "panel",
                "particle",
                "particleemitter",
                "texture",
                "usermsg",
                "convar",
                "mesh",
                "matrix",
                "sound",
                "pixelvishandle",
                "dlight",
                "video",
                "file",
                "locomotion",
                "path",
                "navarea",
                "soundhandle",
                "navladder",
                "particlesystem",
                "projectedtexture",

                0
            };
#endif
        }
    }
}

#endif
