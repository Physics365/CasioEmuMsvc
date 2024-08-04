#include "Logger.hpp"

#include <stdio.h>
#include <stdarg.h>
#include <Windows.h>

namespace casioemu
{
	namespace logger
	{
		void Info(const char *format, ...)
		{
			va_list args;
			va_start(args, format);
			printf(format, args);
			va_end(args);
			
		}
		void Report(const char* what) {
			MessageBoxA(0, what, "CasioEmu", 0);
		}
	}
}

