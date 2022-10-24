#include "platforms/platform.h"
#include "platforms/log.h"

#include <locale.h>
#include <signal.h>
#include <stdlib.h>

//Handle crash signals

void sigFunc(int signal) {

	const C8 *msg = "Undefined instruction";;

	switch (signal) {
		case SIGABRT:	msg = "Abort was called";					break;
		case SIGFPE:	msg = "Floating point error occurred";		break;
		case SIGILL:	msg = "Illegal instruction";				break;
		case SIGINT:	msg = "Interrupt was called";				break;
		case SIGSEGV:	msg = "Segfault";							break;
		case SIGTERM:	msg = "Terminate was called";				break;
	}

	//Outputting to console is not technically allowed by the Windows docs
	//If this signal is triggered from the wrong thread it might cause stackoverflow, 
	//but what are you gonna do? Crash again?
	//For debugging purposed however, this is very useful
	//Turn this off by defining _NO_SIGNAL_HANDLING

	Log_log(LogLevel_Fatal, LogOptions_Default, (struct LogArgs) { .argc = 1, .args = &msg });
	Log_printStackTrace(1, LogLevel_Fatal);
	exit(signal);
}

void *allocCallback(void *allocator, U64 siz) {
	allocator;
	return malloc(siz);
}

void freeCallback(void *allocator, struct Buffer buf) {
	allocator;
	free(buf.ptr);
}

int main(int argc, const char *argv[]) {
	
	#ifndef _NO_SIGNAL_HANDLING
		signal(SIGABRT, sigFunc);
		signal(SIGFPE,	sigFunc);
		signal(SIGILL,	sigFunc);
		signal(SIGINT,	sigFunc);
		signal(SIGSEGV, sigFunc);
		signal(SIGTERM, sigFunc);
	#endif

	Platform_create(argc, argv, GetModuleHandleA(NULL), allocCallback, freeCallback, NULL);

	int res = Program_run();
	Program_exit();
	Program_cleanup();

	return res;
}