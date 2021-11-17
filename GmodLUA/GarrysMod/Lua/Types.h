#ifndef GARRYSMOD_LUA_TYPES_H
#define GARRYSMOD_LUA_TYPES_H

namespace GarrysMod
{
    namespace Lua
    {
        namespace Type
        {
            enum
            {
#ifdef GMOD_ALLOW_DEPRECATED
                // Deprecated: Use `None` instead of `Invalid`
                Invalid = -1,
#endif

                // Default Lua Types
                None = -1,
                Nil,
                Bool,
                LightUserData,
                Number,
                String,
                Table,
                Function,
                UserData,
                Thread,

                // GMod Types
                Entity,
                Vector,
                Angle,
                PhysObj,
                Save,
                Restore,
                DamageInfo,
                EffectData,
                MoveData,
                RecipientFilter,
                UserCmd,
                ScriptedVehicle,
                Material,
                Panel,
                Particle,
                ParticleEmitter,
                Texture,
                UserMsg,
                ConVar,
                IMesh,
                Matrix,
                Sound,
                PixelVisHandle,
                DLight,
                Video,
                File,
                Locomotion,
                Path,
                NavArea,
                SoundHandle,
                NavLadder,
                ParticleSystem,
                ProjectedTexture,
                PhysCollide,
                SurfaceInfo,

                Type_Count
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
                "physcollide",
                "surfaceinfo",
                nullptr
            };
#endif
        }
    }
}

#endif
