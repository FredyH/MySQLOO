#define _CRT_SECURE_NO_DEPRECATE
#include "Logger.h"
#include <string>
#include <mutex>
#include <cstdio>
#include <stdarg.h>

namespace Logger
{
	static std::mutex loggerMutex;
	static FILE* logFile;
	static bool loggingFailed = false;

	//Disables logging
	static void disableLogging(const char* message)
	{
		if (logFile != NULL)
		{
			fclose(logFile);
			logFile = NULL;
		}
		loggingFailed = true;
		printf("%s\n", message);
	}

	//Attempts to initialize the logfile stream
	//If it fails it disables logging
	static bool initFileStream()
	{
		logFile = fopen("mysqloo.log", "a");
		if (logFile == NULL)
		{
			disableLogging("Logger failed to open log file, logging disabled");
			return false;
		}
		return true;
	}

	//Logs a message to mysqloo.log, if it fails logging will be disabled
	void Log(const char* format, ...)
	{
#ifdef LOGGER_ENABLED
		//Double check in case one thread doesn't know about the change to loggingFailed yet,
		//but to increase performance in case it already does
		if (loggingFailed) return;
		std::lock_guard<std::mutex> lock(loggerMutex);
		if (loggingFailed) return;
		if (logFile == NULL)
		{
			if (!initFileStream())
			{
				return;
			}
		}
		va_list varArgs;
		va_start(varArgs, format);
		int result = vfprintf(logFile, format, varArgs);
		va_end(varArgs);
		if (result < 0)
		{
			disableLogging("Failed to write to log file");
		}
		else if (fflush(logFile) < 0)
		{
			disableLogging("Failed to flush to log file");
		}
#endif
	}
}