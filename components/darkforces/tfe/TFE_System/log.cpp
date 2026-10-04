// ESP32 (esp-box-emu) logging: write to the serial console only. The desktop
// version also mirrors everything to a log file, but flushing a file on the SD
// card for every message is far too slow here.
#include <cstdarg>
#include <cstring>
#include <cstdio>

#include <TFE_System/system.h>

namespace TFE_System
{
	static char s_msgStr[1024];
	static const char* c_typeNames[]=
	{
		"",			//LOG_MSG = 0,
		"Warning",	//LOG_WARNING,
		"Error",	//LOG_ERROR,
		"Critical", //LOG_CRITICAL,
	};

	bool logOpen(const char* filename)
	{
		(void)filename;
		return true;
	}

	void logClose()
	{
	}

	void debugWrite(const char* tag, const char* str, ...)
	{
		if (!tag || !str) { return; }

		va_list arg;
		va_start(arg, str);
		vsnprintf(s_msgStr, sizeof(s_msgStr), str, arg);
		va_end(arg);

		printf("[%s] %s\n", tag, s_msgStr);
	}

	void logWrite(LogWriteType type, const char* tag, const char* str, ...)
	{
		if (type >= LOG_COUNT || !tag || !str) { return; }

		va_list arg;
		va_start(arg, str);
		vsnprintf(s_msgStr, sizeof(s_msgStr), str, arg);
		va_end(arg);

		// Strip a trailing newline, one is always added below.
		size_t len = strlen(s_msgStr);
		while (len && (s_msgStr[len - 1] == '\n' || s_msgStr[len - 1] == '\r')) { s_msgStr[--len] = 0; }

		if (type != LOG_MSG)
		{
			printf("[TFE %s : %s] %s\n", c_typeNames[type], tag, s_msgStr);
		}
		else
		{
			printf("[TFE %s] %s\n", tag, s_msgStr);
		}
	}
}
