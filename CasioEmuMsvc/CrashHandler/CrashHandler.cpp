#include <iostream>
#include <windows.h>

#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

void CreateMiniDump(EXCEPTION_POINTERS* pExceptionPointers);
LONG WINAPI CustomUnhandledExceptionFilter(EXCEPTION_POINTERS* pExceptionPointers);
void PrintStackTrace(EXCEPTION_POINTERS* pExceptionPointers);

class GlobalCrashHandler {
public:
	GlobalCrashHandler() {
		AddVectoredExceptionHandler(1, CustomUnhandledExceptionFilter);
		SetUnhandledExceptionFilter(CustomUnhandledExceptionFilter);
	}
} g_crashhandler;

LONG WINAPI CustomUnhandledExceptionFilter(EXCEPTION_POINTERS* pExceptionPointers) {
	if (IsDebuggerPresent())
		return EXCEPTION_CONTINUE_SEARCH;
	auto code = pExceptionPointers->ExceptionRecord->ExceptionCode;
	if (code < 0x80000000)
		return EXCEPTION_CONTINUE_SEARCH;

	CreateMiniDump(pExceptionPointers);

	std::cerr << "\n\n\n!!!\n\nCasioEmuMsvc crashed!\n";
	std::cerr << "Exception code: 0x" << std::hex << pExceptionPointers->ExceptionRecord->ExceptionCode << "\n";
	std::cerr << "Exception address: 0x" << std::hex << pExceptionPointers->ExceptionRecord->ExceptionAddress << "\n";

	PrintStackTrace(pExceptionPointers);

	std::cerr << "Core dumped.\n";
	std::cerr << "Press any key to close...\n";
	std::cerr.flush();
	std::cin.get();
	TerminateProcess(GetCurrentProcess(), pExceptionPointers->ExceptionRecord->ExceptionCode);
	return EXCEPTION_EXECUTE_HANDLER;
}

void CreateMiniDump(EXCEPTION_POINTERS* pExceptionPointers) {
	HANDLE hFile = CreateFile(TEXT("crashdump.dmp"), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile != INVALID_HANDLE_VALUE) {
		MINIDUMP_EXCEPTION_INFORMATION mdei;
		mdei.ThreadId = GetCurrentThreadId();
		mdei.ExceptionPointers = pExceptionPointers;
		mdei.ClientPointers = FALSE;

		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, pExceptionPointers ? &mdei : NULL, NULL, NULL);

		CloseHandle(hFile);
	}
}

void PrintStackTrace(EXCEPTION_POINTERS* pExceptionPointers) {
	HANDLE hProcess = GetCurrentProcess();
	HANDLE hThread = GetCurrentThread();

	CONTEXT context = *pExceptionPointers->ContextRecord;

	SymInitialize(hProcess, NULL, TRUE);

	STACKFRAME64 stackFrame;
	ZeroMemory(&stackFrame, sizeof(STACKFRAME64));

	DWORD machineType = IMAGE_FILE_MACHINE_I386;
#ifdef _M_X64
	machineType = IMAGE_FILE_MACHINE_AMD64;
	stackFrame.AddrPC.Offset = context.Rip;
	stackFrame.AddrPC.Mode = AddrModeFlat;
	stackFrame.AddrFrame.Offset = context.Rsp;
	stackFrame.AddrFrame.Mode = AddrModeFlat;
	stackFrame.AddrStack.Offset = context.Rsp;
	stackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_IX86
	machineType = IMAGE_FILE_MACHINE_I386;
	stackFrame.AddrPC.Offset = context.Eip;
	stackFrame.AddrPC.Mode = AddrModeFlat;
	stackFrame.AddrFrame.Offset = context.Ebp;
	stackFrame.AddrFrame.Mode = AddrModeFlat;
	stackFrame.AddrStack.Offset = context.Esp;
	stackFrame.AddrStack.Mode = AddrModeFlat;
#endif

	while (StackWalk64(machineType, hProcess, hThread, &stackFrame, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
		DWORD64 address = stackFrame.AddrPC.Offset;

		if (address == 0) {
			break;
		}

		DWORD64 displacementSym = 0;
		DWORD64 displacementLine = 0;

		char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
		PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
		pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		pSymbol->MaxNameLen = MAX_SYM_NAME;

		if (SymFromAddr(hProcess, address, &displacementSym, pSymbol)) {
			std::cerr << " Function: " << pSymbol->Name << "(0x" << std::hex << address << ")" << std::dec;
		}
		else {
			std::cerr << std::hex << address << std::dec;
		}

		IMAGEHLP_LINE64 line;
		line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

		if (SymGetLineFromAddr64(hProcess, address, (PDWORD)&displacementLine, &line)) {
			std::cerr << " (" << line.FileName << ":" << line.LineNumber << ")";
		}
		std::cerr << '\n';
	}

	SymCleanup(hProcess);
}