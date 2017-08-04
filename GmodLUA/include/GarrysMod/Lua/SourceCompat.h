#ifndef GARRYSMOD_LUA_SOURCECOMPAT_H
#define GARRYSMOD_LUA_SOURCECOMPAT_H

#ifdef GMOD_USE_SOURCESDK
    #include "mathlib/vector.h"
#else
    struct Vector
    {
        Vector()
            : x( 0.f )
            , y( 0.f )
            , z( 0.f )
        {}

        float x, y, z;
    };

    struct QAngle
    {
        QAngle()
            : x( 0.f )
            , y( 0.f )
            , z( 0.f )
        {}

        float x, y, z;
    };
#endif

#endif
