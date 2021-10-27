set(SOURCES
    GarrysMod/Lua/Interface.h
    GarrysMod/Lua/LuaBase.h
    GarrysMod/Lua/SourceCompat.h
    GarrysMod/Lua/Types.h
    GarrysMod/Lua/UserData.h)

add_library(gmod-module-base INTERFACE)
target_include_directories(gmod-module-base INTERFACE ./)

function(set_gmod_suffix_prefix library)
	SET_TARGET_PROPERTIES(${library} PROPERTIES PREFIX "gmsv_")

	if(APPLE)
		if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
			SET_TARGET_PROPERTIES(${library} PROPERTIES SUFFIX "_osx.dll")
		else()
			SET_TARGET_PROPERTIES(${library} PROPERTIES SUFFIX "_osx64.dll")
		endif()
	elseif(UNIX)
		if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
			SET_TARGET_PROPERTIES(${library} PROPERTIES SUFFIX "_linux.dll")
		else()
			SET_TARGET_PROPERTIES(${library} PROPERTIES SUFFIX "_linux64.dll")
		endif()
	elseif(WIN32)
		if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
			SET_TARGET_PROPERTIES(${library} PROPERTIES SUFFIX "_win32.dll")
		else()
			SET_TARGET_PROPERTIES(${library} PROPERTIES SUFFIX "_win64.dll")
		endif()
	endif()
endfunction()