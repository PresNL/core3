#include "platforms/log.h"
#include "platforms/thread.h"
#include "platforms/platform.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "types/time.h"

//Unfortunately before Windows 10 it doesn't support printing colors into console using printf
//We also use Windows dependent stack tracing

#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <signal.h>
#include <DbgHelp.h>
#include <stdio.h>

#pragma comment(lib, "DbgHelp.lib")

//Carried over from core2

void Log_captureStackTrace(void **stack, U64 stackSize, U64 skip) {
	RtlCaptureStackBackTrace((DWORD)(1 + skip), (DWORD) stackSize, stack, NULL);
}

typedef struct CapturedStackTrace {

	//Module and symbol

	String mod, sym;

	//File and line don't have to be specified, for external calls

	String fil;
	U32 lin;

} CapturedStackTrace;

static const WORD COLORS[] = {
	2,	/* green */
	3,	/* cyan */
	14,	/* yellow */
	4,	/* red */
	12	/* bright red */
};

void Log_printCapturedStackTraceCustom(const void **stackTrace, U64 stackSize, ELogLevel lvl, ELogOptions opt) {

	if(lvl == ELogLevel_Fatal)
		lvl = ELogLevel_Error;

	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(handle, COLORS[lvl]);

	CapturedStackTrace captured[_STACKTRACE_SIZE] = { 0 };

	U64 stackCount = 0;

	//Obtain process

	HANDLE process = GetCurrentProcess();
	HMODULE processModule = GetModuleHandleA(NULL);

	Bool hasSymbols = SymInitialize(process, NULL, TRUE);
	Bool anySymbol = false;

	if(hasSymbols)
		for (
			U64 i = 0; 
			i < stackSize && i < _STACKTRACE_SIZE && 
			stackTrace[i] && stackTrace[i] != (void*)0xCCCCCCCCCCCCCCCC; 
			++i, ++stackCount
		) {

			U64 addr = (U64) stackTrace[i];

			//Get module name

			U64 moduleBase = SymGetModuleBase(process, addr);

			C8 modulePath[MAX_PATH + 1] = { 0 };
			if (!moduleBase || !GetModuleFileNameA((HINSTANCE)moduleBase, modulePath, MAX_PATH))
				continue;

			anySymbol = true;

			C8 symbolData[sizeof(IMAGEHLP_SYMBOL) + MAX_PATH + 1] = { 0 };

			PIMAGEHLP_SYMBOL symbol = (PIMAGEHLP_SYMBOL)symbolData;
			symbol->SizeOfStruct = sizeof(symbolData);
			symbol->MaxNameLength = MAX_PATH;

			C8 *symbolName = symbol->Name;

			if (!SymGetSymFromAddr(process, addr, NULL, symbol))
				continue;

			DWORD offset = 0;
			IMAGEHLP_LINE line = { 0 };
			line.SizeOfStruct = sizeof(line);

			SymGetLineFromAddr(process, addr, &offset, &line);	//Can fail, meaning that line is null

			if (line.FileName && strlen(line.FileName) > MAX_PATH)
				continue;

			CapturedStackTrace *capture = captured + i;
			capture->mod = String_createRef(modulePath, MAX_PATH);
			capture->sym = String_createRef(symbolName, MAX_PATH);

			String_formatPath(&capture->sym);

			if (moduleBase == (U64)processModule)
				capture->mod = String_getFilePath(&capture->mod);

			if(line.FileName)
				capture->fil = String_createConstRef(line.FileName, MAX_PATH);

			//Copy strings to heap, since they'll go out of scope

			Error err;

			if(capture->mod.length)
				if ((err = String_createCopyx(capture->mod, &capture->mod)).genericError)
					goto cleanup;

			if(capture->sym.length)
				if ((err = String_createCopyx(capture->sym, &capture->sym)).genericError)
					goto cleanup;

			if(capture->fil.length)
				if ((err = String_createCopyx(capture->fil, &capture->fil)).genericError)
					goto cleanup;

			capture->lin = line.LineNumber;
			continue;

			//Cleanup the stack if we can't move anything to heap anymore

		cleanup:
			
			for (U64 j = 0; j < i; ++j) {
				String_freex(&captured[j].fil);
				String_freex(&captured[j].sym);
				String_freex(&captured[j].mod);
			}

			Error_printx(err, lvl, opt);
			return;
		}

	if(hasSymbols && anySymbol)
		printf("Stacktrace:\n");

	else printf("Stacktrace: (No symbols)\n");

	for (U64 i = 0; i < stackCount; ++i) {

		CapturedStackTrace capture = captured[i];

		if(!capture.sym.length)
			printf("%p\n", stackTrace[i]);

		else if(capture.lin)
			printf(
				"%p: %.*s!%.*s (%.*s, Line %u)\n", 
				stackTrace[i], 
				(int) U64_min(I32_MAX, capture.mod.length), capture.mod.ptr, 
				(int) U64_min(I32_MAX, capture.sym.length), capture.sym.ptr,
				(int) U64_min(I32_MAX, capture.fil.length), capture.fil.ptr, 
				capture.lin
			);

		else printf(
			"%p: %.*s!%.*s\n", 
			stackTrace[i], 
			(int) U64_min(I32_MAX, capture.mod.length), capture.mod.ptr, 
			(int) U64_min(I32_MAX, capture.sym.length), capture.sym.ptr
		);

		//We now don't need the strings anymore

		String_freex(&capture.fil);
		String_freex(&capture.sym);
		String_freex(&capture.mod);
	}

	SymCleanup(process);
}

void Log_log(ELogLevel lvl, ELogOptions options, LogArgs args) {

	Ns t = Time_now();

	U32 thread = Thread_getId();

	//Prepare for message

	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(handle, COLORS[lvl]);

	//[<thread> <time>]: <hr\n><ourStuff> <\n if enabled>

	Bool hasTimestamp = options & ELogOptions_Timestamp;
	Bool hasThread = options & ELogOptions_Thread;
	Bool hasNewLine = options & ELogOptions_NewLine;
	Bool hasPrepend = hasTimestamp || hasThread;

	if (hasPrepend)
		printf("[");

	if (hasThread) {

		LongString str;
		Log_num10(str, thread);

		printf("%s", str);
	}

	if (hasTimestamp) {

		TimerFormat tf;
		Time_format(t, tf);

		printf("%s%s", hasThread ? " " : "", tf);
	}

	if (hasPrepend)
		printf("]: ");

	//Print to console and debug window

	const C8 *newLine = hasNewLine ? "\n" : "";

	for (U64 i = 0; i < args.argc; ++i)
		printf(
			"%.*s%s", 
			(int) U64_min(I32_MAX, args.args[i].length), args.args[i].ptr,
			newLine
		);

	//Debug utils such as output to VS

	if (!IsDebuggerPresent())
		return;

	for (U64 i = 0; i < args.argc; ++i) {

		//TODO: This requires null terminator!

		if(args.args[i].ptr)
			OutputDebugStringA(args.args[i].ptr);

		if (hasNewLine)
			OutputDebugStringA("\n");
	}

	if (lvl >= ELogLevel_Error)
		DebugBreak();
}
