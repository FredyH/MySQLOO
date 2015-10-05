#ifndef LOGGER_
#define LOGGER_
#define LOGGER_ENABLED 0
#ifdef LINUX 
#define __FUNCSIG__ __PRETTY_FUNCTION__ 
#endif
#if LOGGER_ENABLED == 1
#define LOG_CURRENT_FUNCTIONCALL Logger::Log("Calling function %s in file %s line:%d\n", __FUNCSIG__, __FILE__, __LINE__);
#else
#define LOG_CURRENT_FUNCTIONCALL ;
#endif

#include <string>

namespace Logger
{
	void Log(const char* format, ...);
};

#endif