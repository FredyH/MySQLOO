cmake_minimum_required(VERSION 3.10)

if(VCPKG_TARGET_TRIPLET MATCHES "^x86-" AND UNIX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32" CACHE STRING "" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32" CACHE STRING "" FORCE)
endif()

project(mysqloo)
add_subdirectory(GmodLUA)

file(GLOB_RECURSE MYSQLOO_SRC "src/*.h" "src/*.cpp")
set(SOURCE_FILES ${MYSQLOO_SRC})
set(CMAKE_BUILD_TYPE RelWithDebInfo)
set (CMAKE_CXX_STANDARD 14)

find_package(OpenSSL REQUIRED)
find_package(unofficial-libmariadb CONFIG REQUIRED)

add_library(mysqloo SHARED ${SOURCE_FILES})

target_link_libraries(mysqloo gmod-module-base unofficial::libmariadb)

if(WIN32)
    target_link_libraries(mysqloo ws2_32 shlwapi bcrypt secur32 crypt32)
else ()
    find_package(Threads REQUIRED)
    target_link_libraries(mysqloo OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS})
    target_link_libraries(mysqloo -static-libstdc++)
endif()


set_gmod_suffix_prefix(mysqloo)